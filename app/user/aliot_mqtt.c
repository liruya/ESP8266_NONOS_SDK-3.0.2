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
#include "udpserver.h"
#include "user_rtc.h"
#include "user_code.h"

typedef struct {
    void (* mqtt_connect_cb)();

    void (* sntp_response_cb)(const uint64_t time);
    void (* property_set_cb)(const char *msgid, cJSON *params);
    void (* async_service_cb)(const char *msgid, const char *service_id, cJSON *params);
    void (* sync_service_cb)(const char *rrpcid, const char *msgid, const char *service_id, cJSON *params);
    void (* custom_cb)(const char *custom_id, cJSON *params);
} aliot_callback_t;

static const char *TAG = "Mqtt";

static bool mqttConnected;
static dev_meta_info_t *meta;
static MQTT_Client mqttClient;


static void parse_devlabel_update_reply(const char *topic, const char *data, uint32_t data_len);
static void parse_devlabel_delete_reply(const char *topic, const char *data, uint32_t data_len);
static void parse_property_post_reply(const char *topic, const char *data, uint32_t data_len);

static void parse_sntp_response(const char *topic, const char *data, uint32_t data_len);
static void parse_property_set(const char *topic, const char *data, uint32_t data_len);
static void parse_async_service(const char *topic, const char *data, uint32_t data_len);
static void parse_sync_service(const char *topic, const char *data, uint32_t data_len);
static void parse_custom(const char *topic, const char *data, uint32_t data_len);

typedef struct {
    const char *topic_fmt;
    void (*parse_cb)(const char *topic, const char *data, uint32_t data_len);
} recv_topic_map_t; 

static const recv_topic_map_t recv_topic_map[] = {
    {
        "/ext/ntp/+/+/response",
        parse_sntp_response
    },
    {
        "/sys/+/+/thing/service/property/set",
        parse_property_set
    },
    {
        "/sys/+/+/thing/service/+",
        parse_async_service
    },
    {
        "/sys/+/+/rrpc/request/+",
        parse_sync_service
    },
    {
        "/+/+/user/#",
        parse_custom
    }
};

static aliot_callback_t aliot_callback;

ICACHE_FLASH_ATTR int get_topic_level_count(char *topic, uint32_t topic_len) {
    if (topic == NULL) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < topic_len; i++) {
        if (topic[i] == '/') {
            count++;
        }
    }
    return count;
}

ICACHE_FLASH_ATTR bool get_topic_level_name(char *topic, uint32_t topic_len, int level, char **name) {
    if (topic == NULL || name == NULL) {
        return false;
    }
    int idx = 0;
    char *pstart = NULL;
    char *pend = NULL;
    for (int i = 0; i < topic_len; i++) {
        if (topic[i] == '/') {
            idx++;
            if (idx == level) {
                pstart = &topic[i+1];
            } else if (idx == level + 1) {
                pend = &topic[i];
                break;
            }
        }
    }
    if (pstart == NULL) {
        return false;
    }
    if (pend = NULL) {
        pend = topic + topic_len;
    }
    int len = pend - pstart;
    char *pname = (char *) os_zalloc(len+1);
    if (pname == NULL) {
        return false;
    }
    os_memcpy(pname, pstart, len);
    *name = pname;

    return true;
}

ICACHE_FLASH_ATTR bool check_topic_match(const char *fmt, const char *topic) {
    if (fmt == NULL || topic == NULL 
        || os_strlen(fmt) < 2 || os_strlen(topic) < 2
        || fmt[0] != '/' || topic[0] != '/') {
        return false;
    }

    int i = 1, j = 1;
    int idx1 = 1, idx2 = 1;
    int target = 1;
    const char *ps1 = &fmt[1];
    const char *pe1 = NULL;
    const char *ps2 = &topic[1];
    const char *pe2 = NULL;
    int len1 = 0;
    int len2 = 0;

    while (1) {
        while (i < os_strlen(fmt)) {
            if (fmt[i] == '/') {
                idx1++;
                if (idx1 == target+1) {
                    pe1 = &fmt[i++];
                    break;
                }
            }
            i++;
        }
        if (pe1 == NULL) {
            i = os_strlen(fmt);
            len1 = fmt + i - ps1;
        } else {
            len1 = pe1 - ps1;
        }

        while (j < os_strlen(topic)) {
            if (topic[j] == '/') {
                idx2++;
                if (idx2 == target+1) {
                    pe2 = &topic[j++];
                    break;
                }
            }
            j++;
        }
        if (pe2 == NULL) {
            j = os_strlen(topic);
            len2 = topic + j - ps2;
        } else {
            len2 = pe2 - ps2;
        }

        if (len1 == 0 || len2 == 0) {
            return false;
        }

        if (pe1 != NULL) {
            /* pe1 != NULL && pe2 == NULL */
            if (pe2 == NULL) {
                return false;
            }
            /* pe1 != NULL && pe2 != NULL */
            if ((len1 == 1 && (*ps1) == '+')
                || (len1 == len2 && os_memcmp(ps1, ps2, len1) == 0)) {
                target++;
                ps1 = &fmt[i];
                pe1 = NULL;
                ps2 = &topic[j];
                pe2 = NULL;
                continue;
            }
            return false;
        }
        /* pe1 == NULL && pe2 != NULL */
        if (pe2 != NULL) {
            return (len1 == 1 && (*ps1) == '#');
        }
        /* pe1 == NULL && pe2 == NULL */
        if (len1 == 1 && ((*ps1) == '+' || (*ps1) == '#')) {
            return true;
        }
        return (len1 == len2 && os_memcmp(ps1, ps2, len1) == 0);
    }
    return false;
}

/*********************************************  DEVLABEL  *********************************************/
#ifdef  DEVLABEL_ENABLED
ICACHE_FLASH_ATTR static void parse_devlabel_update_reply(const char *topic, const char *data, uint32_t data_len) {
    
}

ICACHE_FLASH_ATTR static void parse_devlabel_delete_reply(const char *topic, const char *data, uint32_t data_len) {

}

#define DEVLABEL_UPDATE_FMT     "{\
\"id\":\"%s\",\
\"version\":\"1.0\",\
\"method\":\"thing.deviceinfo.update\",\
\"params\":["
ICACHE_FLASH_ATTR void aliot_mqtt_update_label(key_value_t *lables, const uint8_t size) {
    if (lables == NULL || size == 0 || size > 16) {
        return;
    }
    // 16*128
    char *payload = (char*) os_zalloc(2560);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_sprintf(payload, DEVLABEL_UPDATE_FMT, aliot_mqtt_getid());
    int i;
    key_value_t *plabel = lables;
    for (i = 0; i < size; i++) {
        os_sprintf(payload+os_strlen(payload), "{\"attrKey\":\"%s\",\"attrValue\":\"%s\"},", plabel->key, plabel->value);
        plabel++;
    }
    *(payload+os_strlen(payload)-1) = ']';
    *(payload+os_strlen(payload)) = '}';
    aliot_mqtt_publish(TOPIC_DEVLABLE_UPDATE, payload, 0);
    os_free(payload);
    payload = NULL;
    os_delay_us(10000);
}

#define DEVLABEL_DELETE_FMT     "{\
\"id\":\"%s\",\
\"version\":\"1.0\",\
\"method\":\"thing.deviceinfo.delete\",\
\"params\":["
ICACHE_FLASH_ATTR void aliot_mqtt_delete_label(const char **keys, const uint8_t size) {
    if (keys == NULL || size == 0 || size > 16) {
        return;
    }
    // 16*30
    char *payload = (char*) os_zalloc(640);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_sprintf(payload, DEVLABEL_DELETE_FMT, aliot_mqtt_getid());
    int i;
    for (i = 0; i < size; i++) {
        os_sprintf(payload+os_strlen(payload), "{\"attrKey\":\"%s\"},", *keys);
        keys++;
    }
    *(payload+os_strlen(payload)-1) = ']';
    *(payload+os_strlen(payload)) = '}';
    aliot_mqtt_publish(TOPIC_DEVLABLE_DELETE, payload, 0);
    os_free(payload);
    payload = NULL;
    os_delay_us(10000);
}
#endif
/*********************************************  DEVLABEL End*********************************************/


ICACHE_FLASH_ATTR static void parse_sntp_response(const char *topic, const char *data, uint32_t data_len) {
    if (aliot_callback.sntp_response_cb == NULL) {
        return;    
    }
    cJSON *root = cJSON_ParseWithOpts(data, 0, 0);
    do {
        if (!cJSON_IsObject(root)) {
            break;
        }
        cJSON *deviceSend = cJSON_GetObjectItem(root, "deviceSendTime");
        cJSON *serverRecv = cJSON_GetObjectItem(root, "serverRecvTime");
        cJSON *serverSend = cJSON_GetObjectItem(root, "serverSendTime");
        if (!cJSON_IsNumber(deviceSend) || !cJSON_IsNumber(serverRecv) || !cJSON_IsNumber(serverSend)) {
            break;
        }
        uint64_t devSendTime = deviceSend->valuedouble;
        uint64_t servRecvTime = serverRecv->valuedouble;
        uint64_t servSendTime = serverSend->valuedouble;
        uint64_t devRecvTime = system_get_time();
        uint64_t result = (servRecvTime + servSendTime + ((0xFFFFFFFF-devSendTime+devRecvTime+1)&0xFFFFFFFF)/1000)/2;
        
        aliot_callback.sntp_response_cb(result);
    } while (0);
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR static void parse_property_post_reply(const char *topic, const char *data, uint32_t data_len) {

}

ICACHE_FLASH_ATTR static void parse_property_set(const char *topic, const char *data, uint32_t data_len) {
    if (aliot_callback.property_set_cb == NULL) {
        return;
    }
    cJSON *root = cJSON_ParseWithOpts(data, 0, 0);
    do {
        if (!cJSON_IsObject(root)) {
            break;
        }
        
        cJSON *params = cJSON_GetObjectItem(root, "params");
        if (!cJSON_IsObject(params)) {
            break;
        }
        cJSON *msgid = cJSON_GetObjectItem(root, "id");
        if (!cJSON_IsString(msgid)) {
            break;
        }

        aliot_callback.property_set_cb(msgid->valuestring, params);
    } while (0);
    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR static void parse_async_service(const char *topic, const char *data, uint32_t data_len) {
    if (aliot_callback.async_service_cb == NULL) {
        return;
    }
    const char *service_id = NULL;
    int idx = 0;
    for (int i = 0; i < os_strlen(topic); i++) {
        if (topic[i] == '/') {
            idx++;
            if (idx == 6) {
                service_id = &topic[i+1];
                break;
            }
        }
    }
    if (service_id == NULL) {
        return;
    }
    cJSON *root = cJSON_ParseWithOpts(data, 0, 0);
    do {
        if (!cJSON_IsObject(root)) {
            break;
        }

        cJSON *msgid = cJSON_GetObjectItem(root, "id");
        if (!cJSON_IsString(msgid)) {
            break;
        }
        
        cJSON *params = cJSON_GetObjectItem(root, "params");
        if (!cJSON_IsObject(params)) {
            break;
        }

        aliot_callback.async_service_cb(msgid->valuestring, service_id, params);
    } while (0);

    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR static void parse_sync_service(const char *topic, const char *data, uint32_t data_len) {
    if (aliot_callback.sync_service_cb == NULL) {
        return;
    }
    const char *rrpcid = NULL;
    int idx = 0;
    for (int i = 0; i < os_strlen(topic); i++) {
        if (topic[i] == '/') {
            idx++;
            if (idx == 6) {
                rrpcid = &topic[i+1];
                break;
            }
        }
    }
    if (rrpcid == NULL) {
        return;
    }
    cJSON *root = cJSON_ParseWithOpts(data, 0, 0);
    do {
        if (!cJSON_IsObject(root)) {
            break;
        }

        cJSON *msgid = cJSON_GetObjectItem(root, "id");
        if (!cJSON_IsString(msgid)) {
            break;
        }
        
        cJSON *params = cJSON_GetObjectItem(root, "params");
        if (!cJSON_IsObject(params)) {
            break;
        }

        cJSON *method = cJSON_GetObjectItem(root, "method");
        if (!cJSON_IsString(method) || os_strstr(method->valuestring, "thing.service.") != method->valuestring) {
            break;
        }
        const char *svc_name = method->valuestring + os_strlen("thing.service.");

        aliot_callback.sync_service_cb(rrpcid, msgid->valuestring, svc_name, params);
    } while (0);

    cJSON_Delete(root);
}

ICACHE_FLASH_ATTR static void parse_custom(const char *topic, const char *data, uint32_t data_len) {
    if (aliot_callback.custom_cb == NULL) {
        return;
    }
    const char *custom_id = NULL;
    int idx = 0;
    for (int i = 0; i < os_strlen(topic); i++) {
        if (topic[i] == '/') {
            idx++;
            if (idx == 4) {
                custom_id = &topic[i+1];
                break;
            }
        }
    }
    if (custom_id == NULL) {
        return;
    }
    cJSON *params = cJSON_ParseWithOpts(data, 0, 0);
    do {
        if (cJSON_IsObject(params) == false) {
            break;
        }
        aliot_callback.custom_cb(custom_id, params);
    } while (0);
    cJSON_Delete(params);
}

/*************************************Async Service****************************************************/

#define SERVICE_REPLY_PAYLOAD_FMT   "{\
\"id\":\"%s\",\
\"code\":%d,\
\"data\":%s\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_async_service_reply(const char *service_id, const char *msgid, int code, const char *data) {
    if (meta == NULL || service_id == NULL || msgid == NULL || data == NULL) {
        return;
    }
    char *payload = (char*) os_zalloc(os_strlen(SERVICE_REPLY_PAYLOAD_FMT) + os_strlen(msgid) + os_strlen(data) + 13);
    if (payload == NULL) {
        LOGE(TAG, "malloc payload failed.");
        return;
    }
    char topic[128] = {0};
    os_sprintf(topic, TOPIC_ASYNC_SERVICE_REPLY, meta->product_key, meta->device_name, service_id);
    os_sprintf(payload, SERVICE_REPLY_PAYLOAD_FMT, msgid, code, data);
    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), 0 , 0);
    os_free(payload);
    payload = NULL;
}
/***********************************Async Service End**************************************************/


/***********************************Sync Service**************************************************/

#define RRPC_RESPONSE_FMT     "{\
\"id\":\"%s\",\
\"code\":%d,\
\"data\":%s\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_sync_service_reply(const char *rrpcid, const char *msgid, int code, char *data) {
    if (meta == NULL || !mqttConnected || rrpcid == NULL || data == NULL) {
        return;
    }
    int payload_len = os_strlen(RRPC_RESPONSE_FMT) + os_strlen(data) + os_strlen(msgid) + 13;
    char *payload = (char *) os_zalloc(payload_len);
    if (payload == NULL) {
        LOGE(TAG, "malloc payload failed.");
        return;
    }
    char topic[128] = {0};
    os_sprintf(topic, TOPIC_SYNC_SERVICE_RESPONSE, meta->product_key, meta->device_name, rrpcid);
    os_sprintf(payload, RRPC_RESPONSE_FMT, msgid, code, data);

    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), 0 , 0);
    LOGD(TAG, "topic-> %s", topic);
    LOGD(TAG, "payload-> %s", payload);
    os_free(payload);
    payload = NULL;
}
/***********************************Sync Service End**************************************************/

/***********************************callbacks*********************************************/
ICACHE_FLASH_ATTR void aliot_regist_mqtt_connect_cb(void (* callback)()) {
    aliot_callback.mqtt_connect_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time)) {
    aliot_callback.sntp_response_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_property_set_cb(void (*callback)(const char *msgid, cJSON *params)) {
    aliot_callback.property_set_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_async_service_cb(void (*callback)(const char *msgid, const char *service_id, cJSON *params)) {
    aliot_callback.async_service_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_sync_service_cb(void (*callback)(const char *rrpcid, const char *msgid, const char *service_id, cJSON *params)) {
    aliot_callback.sync_service_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_custom_cb(void (*callback)(const char *custom_id, cJSON *params)) {
    aliot_callback.custom_cb = callback;
}
/*************************************************************************************************/


ICACHE_FLASH_ATTR char* aliot_mqtt_getid() {
    static uint32_t msgid = 0;
    static char idstr[12] = {0};
    msgid++;
    os_memset(idstr, 0, sizeof(idstr));
    os_sprintf(idstr, "%u", msgid);
    return idstr;
}

ICACHE_FLASH_ATTR void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos) {
    if (meta == NULL || !mqttConnected) {
        return;
    }
    char topic[128] = {0};
    os_sprintf(topic, topic_fmt, meta->product_key, meta->device_name);
    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), qos, 0);
    LOGD(TAG, "topic-> %s", topic);
    LOGD(TAG, "payload-> %s", payload);
}

ICACHE_FLASH_ATTR void aliot_mqtt_subscribe_topic(char *topic, const uint8_t qos) {
    MQTT_Subscribe(&mqttClient, topic, qos);
}

#define FOTA_INFORM_PAYLOAD_FMT     "{\"id\":\"%s\",\"params\":{\"version\":\"%s\"}}"
ICACHE_FLASH_ATTR void aliot_mqtt_report_version(char *version) {
    char payload[128] = {0};
    os_sprintf(payload, FOTA_INFORM_PAYLOAD_FMT, aliot_mqtt_getid(), version);
    aliot_mqtt_publish(TOPIC_FOTA_INFORM, payload, 0);
}

ICACHE_FLASH_ATTR static void uint64_to_str(const uint64_t input, char *output) {
	uint64_t temp = input;
	uint8_t i = 0;
	uint8_t j = 0;
	char buf[24] = {0};
	do {
		buf[i++] = (temp%10) + '0';
		temp = temp/10;
	} while (temp > 0);
    LOGI(TAG, "%s", buf);

	do {
		output[j++] = buf[--i];
	} while (i > 0);
    LOGI(TAG, "%s", output);

	output[j] = '\0';
}

#define SNTP_REQUEST_PAYLOAD_FMT    "{\"deviceSendTime\":%s}"        
ICACHE_FLASH_ATTR void aliot_mqtt_get_sntptime(uint64_t sendTime) {
    char time[24] = {0};
    char payload[64] = {0};
    uint64_to_str(sendTime, time);
    os_sprintf(payload, SNTP_REQUEST_PAYLOAD_FMT, time);
    aliot_mqtt_publish(TOPIC_SNTP_REQUEST, payload, 0);
}

#define PROPERTY_POST_PAYLOAD_FMT   "{\
\"id\":\"%s\",\
\"version\":\"1.0\",\
\"params\":%s,\
\"method\":\"thing.event.property.post\"\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_post_property(const char *msgid, const char *params) {
    int len = os_strlen(PROPERTY_POST_PAYLOAD_FMT) + os_strlen(params) + os_strlen(msgid) + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "zalloc payload failed.");
        return;
    }
    os_sprintf(payload, PROPERTY_POST_PAYLOAD_FMT, msgid, params);
    aliot_mqtt_publish(TOPIC_PROPERTY_POST, payload, 0);
    os_free(payload);
    payload = NULL;
}


/***********************************************************************************************/

ICACHE_FLASH_ATTR void aliot_mqtt_connected_cb(uint32_t *args) {
    mqttConnected = true;
    MQTT_Client* client = (MQTT_Client*)args;
    LOGD(TAG, "Connected");

    if (aliot_callback.mqtt_connect_cb != NULL) {
        aliot_callback.mqtt_connect_cb();
    }
}

ICACHE_FLASH_ATTR void aliot_mqtt_disconnected_cb(uint32_t *args) {
    mqttConnected = false;
    MQTT_Client* client = (MQTT_Client*)args;
    LOGD(TAG, "Disconnected");
}

ICACHE_FLASH_ATTR void aliot_mqtt_published_cb(uint32_t *args) {
    MQTT_Client* client = (MQTT_Client*) args;
    LOGD(TAG, "Published");
}

ICACHE_FLASH_ATTR void aliot_mqtt_timeout_cb(uint32_t *args) {
    MQTT_Client* client = (MQTT_Client*) args;
    LOGD(TAG, "Timeout");
}

ICACHE_FLASH_ATTR void aliot_mqtt_data_cb(uint32_t *args, const char *topic, uint32_t topic_len, const char *data, uint32_t data_len) {
    MQTT_Client* client = (MQTT_Client*) args;

    char *topic_buf = (char *) os_zalloc(topic_len + 1);
    if (topic_buf == NULL) {
        LOGE(TAG, "zalloc topic_buf failed");
        return;
    }
    os_memcpy(topic_buf, topic, topic_len);
    LOGD(TAG, "Receive topic: %s\ndata: %s", topic_buf, data);

    for (int i = 0; i < sizeof(recv_topic_map)/sizeof(recv_topic_map[0]); i++) {
        const recv_topic_map_t *ptopic = &recv_topic_map[i];
        if (ptopic->parse_cb != NULL && check_topic_match(ptopic->topic_fmt, topic_buf)) {
            ptopic->parse_cb(topic_buf, data, data_len);
            break;
        }
    }

    os_free(topic_buf);
    topic_buf = NULL;
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
    MQTT_OnTimeout(&mqttClient, aliot_mqtt_timeout_cb);
}

ICACHE_FLASH_ATTR void aliot_mqtt_deinit() {
    MQTT_DeleteClient(&mqttClient);
}
