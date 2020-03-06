#include "aliot_mqtt.h"
#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "mqtt.h"
#include "aliot_sign.h"
#include "dev_sign.h"
#include "ali_config.h"

static bool mqttConnected;
static dev_meta_info_t *meta;
static MQTT_Client mqttClient;

uint32_t ICACHE_FLASH_ATTR aliot_mqtt_getid() {
    static uint32_t msgid = 0;
    msgid++;
    return msgid;
}

void ICACHE_FLASH_ATTR aliot_mqtt_subscribe_topics() {
    if (meta == NULL) {
        return;
    }
    int i;
    char *topic_str;
    int size;

    for (i = 0; i < sizeof(ALIOT_SUBSCRIBE_TOPICS)/sizeof(ALIOT_SUBSCRIBE_TOPICS[0]); i++) {
        subscribe_topic_t *ptopic = &ALIOT_SUBSCRIBE_TOPICS[i];
        size = os_strlen(ptopic->topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
        topic_str = os_zalloc(size);
        if (topic_str == NULL) {
            os_printf("malooc topic_str failed...\n");
            continue;
        }
        os_snprintf(topic_str, size, ptopic->topic_fmt, meta->product_key, meta->device_name);
        MQTT_Subscribe(&mqttClient, topic_str, ptopic->qos);
        os_free(topic_str);
        topic_str = NULL;
    }
}

void ICACHE_FLASH_ATTR aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos, int retain) {
    if (meta == NULL) {
        return;
    }
    int topic_len = os_strlen(topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
    char *topic = os_zalloc(topic_len);
    if (topic == NULL) {
        os_printf("malloc topic failed.\n");
        return;
    }
    os_snprintf(topic, topic_len, topic_fmt, meta->product_key, meta->device_name);

    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), qos , retain);
    os_printf("topic-> %s\n", topic);
    os_printf("payload-> %s\n", payload);
    os_free(topic);
    topic = NULL;
}

void ICACHE_FLASH_ATTR aliot_mqtt_report_version() {
    const char *data = "{\"id\":\"%d\",\"params\":{\"version\":\"%d\"}}";
    int dat_len = os_strlen(data) + 10 + 5 + 1;
    char *payload = (char*) os_zalloc(dat_len);
    if (payload == NULL) {
        os_printf("malloc payload failed.\n");
        return;
    }
    os_snprintf(payload, dat_len, data, aliot_mqtt_getid(), meta->firmware_version);
    aliot_mqtt_publish(FOTA_TOPIC_INFORM, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}

void ICACHE_FLASH_ATTR aliot_mqtt_get_sntptime() {
    char *data = "{\"deviceSendTime\": %u}";
    int dat_len = os_strlen(data) + 10 + 1;
    char *payload = os_zalloc(dat_len);
	if (payload == NULL) {
		os_printf("malloc payload failed...\n");
		return;
	}
    uint32_t deviceSendTime = system_get_time();
    os_snprintf(payload, dat_len, data, deviceSendTime);
    aliot_mqtt_publish(SNTP_TOPIC_REQUEST, payload, 0, 0);
	os_free(payload);
    payload = NULL;
}

// #define DM_POST_FMT "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":%.*s,\"method\":\"%s\"}"
// #define DM_POST_FMT "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{%s},\"method\":\"thing.event.property.post\"}"
// void ICACHE_FLASH_ATTR aliot_mqtt_post_powerswitch(bool power) {
//     char params[64] = {0};
//     os_snprintf(params, 64, "\"PowerSwitch\":%d", power);
//     int len = os_strlen(DM_POST_FMT) + os_strlen(params) + 12;
//     char *payload = os_zalloc(len);
//     if (payload == NULL) {
//         os_printf("malloc payload failed.\n");
//         return;
//     }
//     os_snprintf(payload, len, DM_POST_FMT, aliot_mqtt_getid(), params);
//     os_printf("%s\n", payload);
//     aliot_mqtt_publish(DEVMODEL_PROPERTY_TOPIC_POST, payload, 0, 0);
// }

void ICACHE_FLASH_ATTR aliot_mqtt_report_fota_progress(const int step, const char *msg) {
    char *data = "{\"id\":\"%d\",\"params\":{\"step\":\"%d\",\"desc\":\"%s\"}}";
    int dat_len = os_strlen(data) + 10 + 10 + os_strlen(msg) + 1;
    char *payload = os_zalloc(dat_len);
    if (payload == NULL) {
        os_printf("malloc payload failed...\n");
        return;
    }
    os_snprintf(payload, dat_len, data, aliot_mqtt_getid(), step, msg);
    aliot_mqtt_publish(FOTA_TOPIC_PROGRESS, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}

void ICACHE_FLASH_ATTR aliot_mqtt_parse(const char *topic, const char *payload) {
	if (meta == NULL) {
		return;
	}
    int i;
    char *topic_str;
    int size;

    for (i = 0; i < sizeof(ALIOT_SUBSCRIBE_TOPICS)/sizeof(ALIOT_SUBSCRIBE_TOPICS[0]); i++) {
        subscribe_topic_t *ptopic = &ALIOT_SUBSCRIBE_TOPICS[i];
        size = os_strlen(ptopic->topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
        topic_str = os_zalloc(size);
        if (topic_str == NULL) {
            os_printf("malloc topic_str failed...\n");
            continue;
        }
        os_snprintf(topic_str, size, ptopic->topic_fmt, meta->product_key, meta->device_name);
        if (os_strcmp(topic, topic_str) == 0 && ptopic->parse_function != NULL) {
            ptopic->parse_function(payload);
        }
        os_free(topic_str);
        topic_str = NULL;
    }
}

void ICACHE_FLASH_ATTR aliot_mqtt_connected_cb(uint32_t *args) {
    mqttConnected = true;
    os_printf("MQTT: Connected\r\n");
    MQTT_Client* client = (MQTT_Client*)args;

    aliot_mqtt_subscribe_topics();
    aliot_mqtt_get_sntptime();
    aliot_mqtt_report_version();
}

void ICACHE_FLASH_ATTR aliot_mqtt_disconnected_cb(uint32_t *args) {
    mqttConnected = false;
    MQTT_Client* client = (MQTT_Client*)args;
    os_printf("MQTT: Disconnected\r\n");
}

void ICACHE_FLASH_ATTR aliot_mqtt_published_cb(uint32_t *args) {
    MQTT_Client* client = (MQTT_Client*)args;
    os_printf("MQTT: Published\r\n");
}

void ICACHE_FLASH_ATTR aliot_mqtt_data_cb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data, uint32_t data_len) {
    char *topicBuf = (char*) os_zalloc(topic_len+1);
    char *dataBuf = (char*) os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;

    os_memcpy(dataBuf, data, data_len);
    dataBuf[data_len] = 0;

    os_printf("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);

    aliot_mqtt_parse(topicBuf, dataBuf);

    os_free(topicBuf);
    os_free(dataBuf);
    topicBuf = NULL;
    dataBuf = NULL;
}

void ICACHE_FLASH_ATTR aliot_mqtt_connect() {
	MQTT_Connect(&mqttClient);
}

void ICACHE_FLASH_ATTR aliot_mqtt_disconnect() {
	MQTT_Disconnect(&mqttClient);
}

bool ICACHE_FLASH_ATTR aliot_mqtt_connect_status() {
    return mqttConnected;
}

void ICACHE_FLASH_ATTR aliot_mqtt_init(dev_meta_info_t *dev_meta) {
	meta = dev_meta;
	dev_sign_mqtt_t sign_mqtt;
    os_memset(&sign_mqtt, 0, sizeof(sign_mqtt));
    ali_mqtt_sign(meta->product_key, meta->device_name, meta->device_secret, meta->region, &sign_mqtt);
    os_printf("hostname: %s\n", sign_mqtt.hostname);
    os_printf("clientid: %s\n", sign_mqtt.clientid);
    os_printf("username: %s\n", sign_mqtt.username);
    os_printf("password: %s\n", sign_mqtt.password);

    MQTT_InitConnection(&mqttClient, sign_mqtt.hostname, sign_mqtt.port, 0);
    MQTT_InitClient(&mqttClient, sign_mqtt.clientid, sign_mqtt.username, sign_mqtt.password, 120, 1);

    MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, aliot_mqtt_connected_cb);
    MQTT_OnDisconnected(&mqttClient, aliot_mqtt_disconnected_cb);
    MQTT_OnPublished(&mqttClient, aliot_mqtt_published_cb);
    MQTT_OnData(&mqttClient, aliot_mqtt_data_cb);
}
