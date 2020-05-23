#include "aliot_mqtt.h"
#include "aliot_attr.h"
#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "mqtt.h"
#include "aliot_sign.h"
#include "dev_sign.h"
#include "cJSON.h"
#include "ota.h"
#include "udpserver.h"

// #define USER_LENGTH_MAX     32

typedef struct {
    void (* connect_cb)();
    void (* fota_upgrade_cb)(const char *version, const char *url);
    void (* sntp_response_cb)(const uint64_t time);
} aliot_callback_t;

static const char *TAG = "Mqtt";

static bool mqttConnected;
static dev_meta_info_t *meta;
static MQTT_Client mqttClient;

// static char setfrom[USER_LENGTH_MAX];
// static char getfrom[USER_LENGTH_MAX];

/*************************************************************************************************
 *  Subscribe topics    
 * **********************************************************************************************/

void parse_fota_upgrade(const char *payload);
void parse_devlabel_update_reply(const char *payload);
void parse_devlabel_delete_reply(const char *payload);
void parse_sntp_response(const char *payload);
void parse_property_post_reply(const char *payload) ;
void parse_property_set(const char *payload);
void parse_custom_get(const char *payload);

static const subscribe_topic_t subTopics[4] = {
	// {
	// 	.topic_fmt 		= FOTA_TOPIC_UPGRADE,
	// 	.qos			= 0,
	// 	.parse_function	= parse_fota_upgrade
	// },
	// {
	// 	.topic_fmt 		= DEVLABLE_TOPIC_UPDATE_REPLY,
	// 	.qos			= 0,
	// 	.parse_function	= parse_devlabel_update_reply
	// },
	// {
	// 	.topic_fmt 		= DEVLABLE_TOPIC_DELETE_REPLY,
	// 	.qos			= 0,
	// 	.parse_function	= parse_devlabel_delete_reply
	// },
    // {
	// 	.topic_fmt 		= DEVMODEL_TOPIC_PROPERTY_POST_REPLY,
	// 	.qos			= 0,
	// 	.parse_function	= parse_property_post_reply
	// },
	{
        .is_custom      = false,
		.topic_fmt 		= SNTP_TOPIC_RESPONSE,
		.qos			= 0,
		.parse_function	= parse_sntp_response
	},
	{
        .is_custom      = false,
		.topic_fmt 		= DEVMODEL_TOPIC_PROPERTY_SET,
		.qos			= 0,
		.parse_function	= parse_property_set
	},
	{
        .is_custom      = true,
		.topic_fmt 		= CUSTOM_TOPIC_GET,
		.qos			= 0,
		.parse_function	= parse_custom_get
	},
    {
        .is_custom      = true,
		.topic_fmt 		= CUSTOM_TOPIC_FOTA_UPGRADE,
		.qos			= 0,
		.parse_function	= parse_fota_upgrade
	}
};

static aliot_callback_t aliot_callback;

ICACHE_FLASH_ATTR void parse_fota_upgrade(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (cJSON_IsObject(root) == false) {
        cJSON_Delete(root);
        return;
    }

    cJSON *msg = cJSON_GetObjectItem(root, "message");
    if (cJSON_IsString(msg) == false || os_strcmp(msg->valuestring, "success") != 0) {
        cJSON_Delete(root);
        return;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (cJSON_IsObject(data) == false) {
        cJSON_Delete(root);
        return;
    }
    cJSON *version = cJSON_GetObjectItem(data, "version");
    if (cJSON_IsString(version) == false) {
        cJSON_Delete(root);
        return;
    }
    cJSON *url = cJSON_GetObjectItem(data, "url");
    if (cJSON_IsString(url) == false) {
        cJSON_Delete(root);
        return;
    }
    LOGD(TAG, "ota version: %s", version->valuestring);
    LOGD(TAG, "ota url: %s", url->valuestring);
    if (aliot_callback.fota_upgrade_cb != NULL) {
        aliot_callback.fota_upgrade_cb(version->valuestring, url->valuestring);
    }
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR void parse_devlabel_update_reply(const char *payload) {
    
}

ICACHE_FLASH_ATTR void parse_devlabel_delete_reply(const char *payload) {

}

ICACHE_FLASH_ATTR void parse_sntp_response(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *deviceSend = cJSON_GetObjectItem(root, "deviceSendTime");
    cJSON *serverRecv = cJSON_GetObjectItem(root, "serverRecvTime");
    cJSON *serverSend = cJSON_GetObjectItem(root, "serverSendTime");
    if (!cJSON_IsNumber(deviceSend) || !cJSON_IsNumber(serverRecv) || !cJSON_IsNumber(serverSend)) {
        cJSON_Delete(root);
        return;
    }
    uint64_t devSendTime = deviceSend->valuedouble;
    uint64_t servRecvTime = serverRecv->valuedouble;
    uint64_t servSendTime = serverSend->valuedouble;
    uint64_t devRecvTime = system_get_time();
    uint64_t result = (servRecvTime + servSendTime + ((0xFFFFFFFF-devSendTime+devRecvTime+1)&0xFFFFFFFF)/1000)/2;
    if (aliot_callback.sntp_response_cb != NULL) {
        aliot_callback.sntp_response_cb(result);
    }
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR void parse_property_post_reply(const char *payload) {

}

ICACHE_FLASH_ATTR void parse_property_set(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!cJSON_IsObject(params)) {
        cJSON_Delete(root);
        return;
    }
    // os_memset(setfrom, 0, sizeof(setfrom));
    // cJSON *from = cJSON_GetObjectItem(root, "userid");
    // if (cJSON_IsString(from) && os_strlen(from->valuestring) < sizeof(setfrom)) {
    //     os_strcpy(setfrom, from->valuestring);
    // }

    aliot_attr_parse_all(params, false);
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR void parse_custom_get(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!cJSON_IsArray(params)) {
        cJSON_Delete(root);
        return;
    }
    // os_memset(getfrom, 0, sizeof(getfrom));
    // cJSON *from = cJSON_GetObjectItem(root, "userid");
    // if (cJSON_IsString(from) && os_strlen(from->valuestring) < sizeof(getfrom)) {
    //     os_strcpy(getfrom, from->valuestring);
    // }

    aliot_attr_parse_get(params, false);
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR void aliot_mqtt_subscribe_topics() {
    if (meta == NULL) {
        return;
    }
    int i;
    char *topic_str;
    int size;

    for (i = 0; i < sizeof(subTopics)/sizeof(subTopics[0]); i++) {
        const subscribe_topic_t *ptopic = &subTopics[i];
        // if (ptopic->is_custom == false) {
        //     continue;
        // }
        size = os_strlen(ptopic->topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
        topic_str = os_zalloc(size);
        if (topic_str == NULL) {
            LOGD(TAG, "malloc topic_str failed...");
            continue;
        }
        os_snprintf(topic_str, size, ptopic->topic_fmt, meta->product_key, meta->device_name);
        MQTT_Subscribe(&mqttClient, topic_str, ptopic->qos);
        os_free(topic_str);
        topic_str = NULL;
        os_delay_us(20000);
    }
}

ICACHE_FLASH_ATTR void aliot_mqtt_unsubscribe_topic(const char *topicFmt) {
    if (meta == NULL) {
        return;
    }
    
    int size = os_strlen(topicFmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
    char *topic_str = os_zalloc(size);
    if (topic_str == NULL) {
        LOGD(TAG, "malloc topic_str failed...");
    } else {
        os_snprintf(topic_str, size, topicFmt, meta->product_key, meta->device_name);
        MQTT_UnSubscribe(&mqttClient, topic_str);
        os_free(topic_str);
        topic_str = NULL;
        os_delay_us(10000);
    }
}

ICACHE_FLASH_ATTR void aliot_regist_fota_upgrade_cb(void (* callback)(const char *ver, const char *url)) {
    aliot_callback.fota_upgrade_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time)) {
    aliot_callback.sntp_response_cb = callback;
}
/*************************************************************************************************/


ICACHE_FLASH_ATTR uint32_t aliot_mqtt_getid() {
    static uint32_t msgid = 0;
    msgid++;
    return msgid;
}

ICACHE_FLASH_ATTR void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos, int retain) {
    if (meta == NULL || !mqttConnected) {
        return;
    }
    int topic_len = os_strlen(topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
    char *topic = os_zalloc(topic_len);
    if (topic == NULL) {
        LOGD(TAG, "malloc topic failed.");
        return;
    }
    os_snprintf(topic, topic_len, topic_fmt, meta->product_key, meta->device_name);

    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), qos , retain);
    LOGD(TAG, "topic-> %s", topic);
    LOGD(TAG, "payload-> %s", payload);
    os_free(topic);
    topic = NULL;
}

// #define DEVLABEL_UPDATE_FMT     "{\
// \"id\":\"%d\",\
// \"version\":\"1.0\",\
// \"method\":\"thing.deviceinfo.update\",\
// \"params\":[{\"attrKey\":\"%s\",\"attrValue\":\"%s\"}]\
// }"
// ICACHE_FLASH_ATTR void aliot_mqtt_update_label(const char *key, const char *value) {
//     int len = os_strlen(DEVLABEL_UPDATE_FMT) + 10 + os_strlen(key) + os_strlen(value) + 1;
//     char *payload = (char*) os_zalloc(len);
//     if (payload == NULL) {
//         LOGD(TAG, "malloc payload failed.");
//         return;
//     }
//     os_snprintf(payload, len, DEVLABEL_UPDATE_FMT, aliot_mqtt_getid(), key, value);
//     aliot_mqtt_publish(DEVLABLE_TOPIC_UPDATE, payload, 0, 0);
//     os_free(payload);
//     payload = NULL;
//     os_delay_us(10000);
// }

//  ${msgid}    ${version}
#define FOTA_INFORM_PAYLOAD_FMT     "{\"id\":\"%d\",\"params\":{\"version\":\"%d\"}}"
ICACHE_FLASH_ATTR void aliot_mqtt_report_version() {
    static bool reported = false;
    if (reported) {
        return;
    }
    int len = os_strlen(FOTA_INFORM_PAYLOAD_FMT) + 10 + 5 + 1;
    char *payload = (char*) os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_snprintf(payload, len, FOTA_INFORM_PAYLOAD_FMT, aliot_mqtt_getid(), meta->firmware_version);
    aliot_mqtt_publish(FOTA_TOPIC_INFORM, payload, 0, 0);
    os_free(payload);
    payload = NULL;
    reported = true;
    os_delay_us(20000);
}

//  ${sendTime}
#define SNTP_REQUEST_PAYLOAD_FMT    "{\"deviceSendTime\":%u}"        
ICACHE_FLASH_ATTR void aliot_mqtt_get_sntptime() {
    int len = os_strlen(SNTP_REQUEST_PAYLOAD_FMT) + 10 + 1;
    char *payload = os_zalloc(len);
	if (payload == NULL) {
		LOGD(TAG, "malloc payload failed...");
		return;
	}
    uint32_t deviceSendTime = system_get_time();
    os_snprintf(payload, len, SNTP_REQUEST_PAYLOAD_FMT, deviceSendTime);
    aliot_mqtt_publish(SNTP_TOPIC_REQUEST, payload, 0, 0);
	os_free(payload);
    payload = NULL;
}

//  ${msgid}    ${params}
#define PROPERTY_POST_PAYLOAD_FMT   "{\
\"id\":\"%d\",\
\"version\":\"1.0\",\
\"params\":{%s},\
\"method\":\"thing.event.property.post\"\
}"
/**
 * @param params: 属性转换后的json格式字符串
 * */
ICACHE_FLASH_ATTR void aliot_mqtt_post_property(const int id, const char *params) {
    // char temp[128];
    // os_memset(temp, 0, sizeof(temp));
    // if (os_strlen(setfrom) > 0) {
    //     os_sprintf(temp, ",\"setfrom\":\"%s\"", setfrom);
    // } else if (os_strlen(getfrom) > 0) {
    //     os_sprintf(temp+os_strlen(temp), ",\"getfrom\":\"%s\"", getfrom);
    // }
    int len = os_strlen(PROPERTY_POST_PAYLOAD_FMT) + os_strlen(params) + 10 + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_snprintf(payload, len, PROPERTY_POST_PAYLOAD_FMT, id, params);
    aliot_mqtt_publish(DEVMODEL_TOPIC_PROPERTY_POST, payload, 0, 0);
    // os_memset(setfrom, 0, sizeof(setfrom));
    // os_memset(getfrom, 0, sizeof(getfrom));
    os_free(payload);
    payload = NULL;
}

//  ${msgid}    ${params}
#define PROPERTY_HISTORY_POST_PAYLOAD_FMT   "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{\"properties\":[%s]},\"method\":\"thing.event.property.history.post\"}"
/**
 * @param params: 属性转换后的json格式字符串
 * */
ICACHE_FLASH_ATTR void aliot_mqtt_post_property_history(const char *params) {
    int len = os_strlen(PROPERTY_HISTORY_POST_PAYLOAD_FMT) + os_strlen(params) + 10 + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_snprintf(payload, len, PROPERTY_HISTORY_POST_PAYLOAD_FMT, aliot_mqtt_getid(), params);
    aliot_mqtt_publish(DEVMODEL_TOPIC_HISTORY_POST, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}

//  ${msgid}    ${progress}     ${descripe}
#define FOTA_PROGRESS_PAYLOAD_FMT   "{\"id\":\"%d\",\"params\":{\"step\":\"%d\",\"desc\":\"%s\"}}"   
ICACHE_FLASH_ATTR void aliot_mqtt_report_fota_progress(const int step, const char *msg) {
    int len = os_strlen(FOTA_PROGRESS_PAYLOAD_FMT) + 10 + 10 + os_strlen(msg) + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed...");
        return;
    }
    os_snprintf(payload, len, FOTA_PROGRESS_PAYLOAD_FMT, aliot_mqtt_getid(), step, msg);
    // aliot_mqtt_publish(FOTA_TOPIC_PROGRESS, payload, 0, 0);
    aliot_mqtt_publish(CUSTOM_TOPIC_FOTA_PROGRESS, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}

ICACHE_FLASH_ATTR static void aliot_mqtt_parse(const char *topic, const char *payload) {
	if (meta == NULL) {
		return;
	}
    int i;
    char *topic_str;
    int size;

    for (i = 0; i < sizeof(subTopics)/sizeof(subTopics[0]); i++) {
        const subscribe_topic_t *ptopic = &subTopics[i];
        size = os_strlen(ptopic->topic_fmt) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + 1;
        topic_str = os_zalloc(size);
        if (topic_str == NULL) {
            LOGD(TAG, "malloc topic_str failed...");
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

ICACHE_FLASH_ATTR void aliot_mqtt_connected_cb(uint32_t *args) {
    mqttConnected = true;
    LOGD(TAG, "Connected");
    MQTT_Client* client = (MQTT_Client*)args;

    aliot_mqtt_subscribe_topics();
    aliot_mqtt_report_version();
    aliot_attr_post_all();

    if (aliot_callback.connect_cb != NULL) {
        aliot_callback.connect_cb();
    }
}

ICACHE_FLASH_ATTR void aliot_mqtt_disconnected_cb(uint32_t *args) {
    mqttConnected = false;
    MQTT_Client* client = (MQTT_Client*)args;
    LOGD(TAG, "Disconnected");
}

ICACHE_FLASH_ATTR void aliot_mqtt_published_cb(uint32_t *args) {
    MQTT_Client* client = (MQTT_Client*)args;
    LOGD(TAG, "Published");
}

ICACHE_FLASH_ATTR void aliot_mqtt_data_cb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data, uint32_t data_len) {
    char *topicBuf = (char*) os_zalloc(topic_len+1);
    char *dataBuf = (char*) os_zalloc(data_len+1);

    MQTT_Client* client = (MQTT_Client*)args;

    os_memcpy(topicBuf, topic, topic_len);
    os_memcpy(dataBuf, data, data_len);

    LOGD(TAG, "Receive topic: %s, data: %s ", topicBuf, dataBuf);

    aliot_mqtt_parse(topicBuf, dataBuf);

    os_free(topicBuf);
    os_free(dataBuf);
    topicBuf = NULL;
    dataBuf = NULL;
}

ICACHE_FLASH_ATTR void aliot_regist_connect_cb(void (*callback)()) {
    aliot_callback.connect_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_mqtt_connect() {
	MQTT_Connect(&mqttClient);
}

ICACHE_FLASH_ATTR void aliot_mqtt_disconnect() {
	MQTT_Disconnect(&mqttClient);
}

ICACHE_FLASH_ATTR bool aliot_mqtt_connect_status() {
    return mqttConnected;
}

ICACHE_FLASH_ATTR void aliot_mqtt_init(dev_meta_info_t *dev_meta) {
	meta = dev_meta;
	dev_sign_mqtt_t sign_mqtt;
    os_memset(&sign_mqtt, 0, sizeof(sign_mqtt));
    ali_mqtt_sign(meta->product_key, meta->device_name, meta->device_secret, meta->region, &sign_mqtt);
    LOGD(TAG, "hostname: %s", sign_mqtt.hostname);
    LOGD(TAG, "clientid: %s", sign_mqtt.clientid);
    LOGD(TAG, "username: %s", sign_mqtt.username);
    LOGD(TAG, "password: %s", sign_mqtt.password);

    MQTT_InitConnection(&mqttClient, sign_mqtt.hostname, sign_mqtt.port, sign_mqtt.security);
    MQTT_InitClient(&mqttClient, sign_mqtt.clientid, sign_mqtt.username, sign_mqtt.password, MQTT_KEEPALIVE, 1);

    MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, aliot_mqtt_connected_cb);
    MQTT_OnDisconnected(&mqttClient, aliot_mqtt_disconnected_cb);
    MQTT_OnPublished(&mqttClient, aliot_mqtt_published_cb);
    MQTT_OnData(&mqttClient, aliot_mqtt_data_cb);
}

ICACHE_FLASH_ATTR void aliot_mqtt_dynregist(dev_meta_info_t *dev_meta) {
	meta = dev_meta;
	dev_sign_mqtt_t sign_mqtt;
    os_memset(&sign_mqtt, 0, sizeof(sign_mqtt));
    ali_mqtt_dynregist(meta->product_key, meta->product_secret, meta->device_name, meta->region, &sign_mqtt);
    // ali_mqtt_sign(meta->product_key, meta->device_name, meta->device_secret, meta->region, &sign_mqtt);
    LOGD(TAG, "hostname: %s", sign_mqtt.hostname);
    LOGD(TAG, "clientid: %s", sign_mqtt.clientid);
    LOGD(TAG, "username: %s", sign_mqtt.username);
    LOGD(TAG, "password: %s", sign_mqtt.password);

    MQTT_InitConnection(&mqttClient, sign_mqtt.hostname, sign_mqtt.port, sign_mqtt.security);
    MQTT_InitClient(&mqttClient, sign_mqtt.clientid, sign_mqtt.username, sign_mqtt.password, MQTT_KEEPALIVE, 1);

    MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, aliot_mqtt_connected_cb);
    MQTT_OnDisconnected(&mqttClient, aliot_mqtt_disconnected_cb);
    MQTT_OnPublished(&mqttClient, aliot_mqtt_published_cb);
    MQTT_OnData(&mqttClient, aliot_mqtt_data_cb);

    MQTT_Connect(&mqttClient);
}
