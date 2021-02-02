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

// #define CONNECT_DELAY       3000
// #define SYNCT_SNTP_TIMEOUT  15

#define UPGRADE_URL_LENMAX          128

#define DEV_CODE_SUCCESS            200
#define DEV_CODE_REQ_ERR            400
#define DEV_CODE_REQ_PARAM_ERR      460
#define DEV_CODE_REQ_BUSY           429

#define ERRMSG_REQUEST              "request error"
#define ERRMSG_REQUEST_PARAM        "request parameter error"
#define ERRMSG_REQUEST_BUSY         "too many requests"

// #define DEVLABEL_ENABLED
#define CUSTOMGET_ENABLED
#define CUSTOMFOTA_ENABLED
#define SERVICE_ENABLED
#define RRPC_ENABLED
// #define COTA_ENABLED

typedef struct {
    bool (* fota_check_cb)();
    char* (* fota_upgrade_cb)(const cJSON *params);
    void (* sntp_response_cb)(const uint64_t time);
} aliot_callback_t;

static const char *TAG = "Mqtt";

static bool mqttConnected;
static dev_meta_info_t *meta;
static MQTT_Client mqttClient;

static bool conn_sync_sntp;

// static os_timer_t conncb_timer;
// static uint8_t check_conn_cnt;

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
void parse_cota_get_reply(const char *payload);
void parse_service(const char *payload);
void parse_rrpc_request(const char *ext, const char *payload);

static const subscribe_topic_t subTopics[] = {
	// {
	// 	.topic_fmt 		= FOTA_TOPIC_UPGRADE,
	// 	.qos			= 0,
	// 	.parse_function	= parse_fota_upgrade
	// },
#ifdef  DEVLABEL_ENABLED
	{
		.topic_fmt 		= DEVLABLE_TOPIC_UPDATE_REPLY,
		.qos			= 0,
		.parse_function	= parse_devlabel_update_reply
	},
	{
		.topic_fmt 		= DEVLABLE_TOPIC_DELETE_REPLY,
		.qos			= 0,
		.parse_function	= parse_devlabel_delete_reply
	},
#endif
    // {
	// 	.topic_fmt 		= DEVMODEL_TOPIC_PROPERTY_POST_REPLY,
	// 	.qos			= 0,
	// 	.parse_function	= parse_property_post_reply
	// },
	{
		.topic_fmt 		= SNTP_TOPIC_RESPONSE,
		.qos			= 0,
		.parse_function	= parse_sntp_response
	},
	{
		.topic_fmt 		= DEVMODEL_TOPIC_PROPERTY_SET,
		.qos			= 0,
		.parse_function	= parse_property_set
	},
#ifdef  CUSTOMGET_ENABLED
	{
		.topic_fmt 		= CUSTOM_TOPIC_GET,
		.qos			= 0,
		.parse_function	= parse_custom_get
	},
#endif
#ifdef  CUSTOMFOTA_ENABLED
    {
		.topic_fmt 		= CUSTOM_TOPIC_FOTA_UPGRADE,
		.qos			= 0,
		.parse_function	= parse_fota_upgrade
	},
#endif
#ifdef  SERVICE_ENABLED
    {
		.topic_fmt 		= DEVMODEL_SERVICE_TOPIC,
		.qos			= 0,
		.parse_function	= parse_service
	},
#endif
#ifdef  RRPC_ENABLED
    {
		.topic_fmt 		= RRPC_TOPIC_REQUEST,
		.qos			= 0,
		.parse_ext_function = parse_rrpc_request
	}
#endif
#ifdef  COTA_ENABLED
    {
        .topic_fmt 		= COTA_TOPIC_GET_REPLY,
		.qos			= 0,
		.parse_function	= parse_cota_get_reply
    }
#endif
};

static aliot_callback_t aliot_callback;

ICACHE_FLASH_ATTR void parse_fota_upgrade(const char *payload) {
    if (aliot_callback.fota_upgrade_cb == NULL) {
        return;
    }
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

    aliot_callback.fota_upgrade_cb(data); 
    
    cJSON_Delete(root);
}

/*********************************************  DEVLABEL  *********************************************/
#ifdef  DEVLABEL_ENABLED
ICACHE_FLASH_ATTR void parse_devlabel_update_reply(const char *payload) {
    
}

ICACHE_FLASH_ATTR void parse_devlabel_delete_reply(const char *payload) {

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
    aliot_mqtt_publish(DEVLABLE_TOPIC_UPDATE, payload, 0, 0);
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
    aliot_mqtt_publish(DEVLABLE_TOPIC_DELETE, payload, 0, 0);
    os_free(payload);
    payload = NULL;
    os_delay_us(10000);
}
#endif
/*********************************************  DEVLABEL End*********************************************/


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
    conn_sync_sntp = true;
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
    cJSON *msgid = cJSON_GetObjectItem(root, "id");
    if (cJSON_IsString(msgid)) {
        aliot_attr_parse_all(msgid->valuestring, params, false);
    } else {
        aliot_attr_parse_all(NULL, params, false);
    }
    // aliot_attr_parse_all(params, false);
    cJSON_Delete(root);
}

#ifdef  CUSTOMGET_ENABLED
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

    aliot_attr_parse_get(params, false);
    cJSON_Delete(root);
}
#endif

/*************************************Service****************************************************/
#ifdef  SERVICE_ENABLED
ICACHE_FLASH_ATTR void parse_service(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *msgid = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsString(msgid)) {
        cJSON_Delete(root);
        return;
    }
    
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!cJSON_IsObject(params)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (!cJSON_IsString(method) || os_strstr(method->valuestring, "thing.service.") != method->valuestring) {
        cJSON_Delete(root);
        return;
    }
    const char *svc_name = method->valuestring + os_strlen("thing.service.");

    cJSON_Delete(root);
}

#define SERVICE_REPLY_PAYLOAD_FMT   "{\
\"id\":\"%s\",\
\"code\":200,\
\"data\":{%s}\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_service_reply(const char *msgid, const char *message) {
    LOGE(TAG, "svc1");
    if (msgid == NULL || message == NULL) {
        return;
    }
    LOGE(TAG, "svc2");
    char *payload = (char*) os_zalloc(os_strlen(SERVICE_REPLY_PAYLOAD_FMT) + os_strlen(message) + 1);
    if (payload == NULL) {
        LOGE(TAG, "malloc payload failed.");
        return;
    }
    LOGE(TAG, "svc3");
    os_sprintf(payload, SERVICE_REPLY_PAYLOAD_FMT, msgid, message);
    aliot_mqtt_publish(DEVMODEL_SERVICE_TOPIC_REPLY, payload, 0, 0);
    os_free(payload);
    payload = NULL;
    os_delay_us(10000);
}

// #define SERVICE_REPLY_PAYLOAD_FMT   "{\
// \"id\":\"%s\",\
// \"code\":200,\
// \"data\":{"
// ICACHE_FLASH_ATTR void aliot_mqtt_service_reply(const char *msgid, const key_value_t *params, const uint8_t size) {
//     if (msgid == NULL || size > 16) {
//         return;
//     }
//     char *payload = (char*) os_zalloc(1024);
//     if (payload == NULL) {
//         LOGD(TAG, "malloc payload failed.");
//         return;
//     }
//     os_sprintf(payload, SERVICE_REPLY_PAYLOAD_FMT, msgid);
//     int idx;
//     if (params == NULL || size == 0) {
//         idx = os_strlen(payload);
//     } else {
//         int i;
//         const key_value_t *par = params;
//         for (i = 0; i < size; i++) {
//             os_sprintf(payload+os_strlen(payload), "\"%s\":\"%s\",", par->key, par->value);
//             par++;
//         }
//         idx = os_strlen(payload)-1;
//     }
//     os_sprintf(payload+idx, "%s", "}}");
//     aliot_mqtt_publish(DEVMODEL_SERVICE_TOPIC_REPLY, payload, 0, 0);
//     os_free(payload);
//     payload = NULL;
//     os_delay_us(10000);
// }
#endif
/***********************************Service End**************************************************/


/***********************************RRPC**************************************************/
#ifdef  RRPC_ENABLED

#define RRPC_RESPONSE_FMT     "{\
\"id\":\"%s\",\
\"code\":%d,\
\"data\":{%s}\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_rrpc_response(const char *msgid, const int code, const char *data) {
    if (meta == NULL || !mqttConnected || msgid == NULL || data == NULL) {
        return;
    }
    int topic_len = os_strlen(RRPC_TOPIC_RESPONSE) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + os_strlen(msgid) + 1;
    int payload_len = os_strlen(RRPC_RESPONSE_FMT) + os_strlen(data) + 12 + 10 + 1;
    char *total = (char *) os_zalloc(topic_len+payload_len);
    if (total == NULL) {
        LOGE(TAG, "malloc topic failed.");
        return;
    }
    char *topic = total;
    char *payload = total + topic_len;
    os_snprintf(topic, topic_len, RRPC_TOPIC_RESPONSE, meta->product_key, meta->device_name, msgid);
    os_snprintf(payload, payload_len, RRPC_RESPONSE_FMT, aliot_mqtt_getid(), code, data);

    MQTT_Publish(&mqttClient, topic, payload, os_strlen(payload), 0 , 0);
    LOGD(TAG, "topic-> %s", topic);
    LOGD(TAG, "payload-> %s", payload);
    os_free(topic);
    topic = NULL;
}

ICACHE_FLASH_ATTR void aliot_mqtt_rrpc_response_fota_upgrade(const char *msgid, const cJSON *params) {
    if (aliot_callback.fota_upgrade_cb == NULL) {
        aliot_mqtt_rrpc_response(msgid, DEV_CODE_REQ_ERR, "");
        return;
    }
    char *result = aliot_callback.fota_upgrade_cb(params);
    aliot_mqtt_rrpc_response(msgid, DEV_CODE_SUCCESS, result);
}

#define FOTA_CHECK_RESPONSE_FMT     "\"version\":%d,\"upgrading\":%d"
ICACHE_FLASH_ATTR void aliot_mqtt_rrpc_response_fota_check(const char *msgid) {
    if (meta == NULL || aliot_callback.fota_check_cb == NULL) {
        aliot_mqtt_rrpc_response(msgid, DEV_CODE_REQ_ERR, "");
        return;
    }
    int len = os_strlen(FOTA_CHECK_RESPONSE_FMT) + 8;
    char *data = (char *) os_zalloc(len);
    if (data == NULL) {
        LOGE(TAG, "malloc data failed.");
        aliot_mqtt_rrpc_response(msgid, DEV_CODE_REQ_ERR, "");
        return;
    }
    bool upgrading = aliot_callback.fota_check_cb();
    os_sprintf(data, FOTA_CHECK_RESPONSE_FMT, meta->firmware_version, upgrading);
    aliot_mqtt_rrpc_response(msgid, DEV_CODE_SUCCESS, data);
    os_free(data);
    data = NULL;
}

#define DEVICE_DATETIME_FMT "\"device_datetime\":\"%s\""
ICACHE_FLASH_ATTR void aliot_mqtt_rrpc_response_datetime(const char *msgid) {
    if (meta == NULL) {
        aliot_mqtt_rrpc_response(msgid, DEV_CODE_REQ_ERR, "");
        return;
    }
    int len = os_strlen(DEVICE_DATETIME_FMT) + os_strlen(meta->device_time) + 1;
    char *data = (char *) os_zalloc(len);
    if (data == NULL) {
        LOGE(TAG, "malloc data failed.");
        aliot_mqtt_rrpc_response(msgid, DEV_CODE_REQ_ERR, "");
        return;
    }
    os_sprintf(data, DEVICE_DATETIME_FMT,  meta->device_time);
    aliot_mqtt_rrpc_response(msgid, DEV_CODE_SUCCESS, data);
    os_free(data);
    data = NULL;
}

ICACHE_FLASH_ATTR void aliot_mqtt_rrpc_response_properties(const char *msgid, const cJSON *params) {
    cJSON *attrKeys = cJSON_GetObjectItem(params, "attrKeys");
    if (!cJSON_IsArray(attrKeys)) {
        return;
    }
    int size = cJSON_GetArraySize(attrKeys);
    if (size == 0) {

    } else {
        
    }
}

ICACHE_FLASH_ATTR void parse_rrpc_request(const char *ext, const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        aliot_mqtt_rrpc_response(ext, DEV_CODE_REQ_PARAM_ERR, "");
        cJSON_Delete(root);
        return;
    }

    cJSON *msgid = cJSON_GetObjectItem(root, "id");
    if (!cJSON_IsString(msgid)) {
        aliot_mqtt_rrpc_response(ext, DEV_CODE_REQ_PARAM_ERR, "");
        cJSON_Delete(root);
        return;
    }
    
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (!cJSON_IsObject(params)) {
        aliot_mqtt_rrpc_response(ext, DEV_CODE_REQ_PARAM_ERR, "");
        cJSON_Delete(root);
        return;
    }

    cJSON *method = cJSON_GetObjectItem(root, "method");
    if (!cJSON_IsString(method) || os_strstr(method->valuestring, "thing.service.") != method->valuestring) {
        aliot_mqtt_rrpc_response(ext, DEV_CODE_REQ_PARAM_ERR, "");
        cJSON_Delete(root);
        return;
    }
    const char *svc_name = method->valuestring + os_strlen("thing.service.");

    if (os_strcmp(svc_name, SVC_FOTA_UPGRADE) == 0) {
        aliot_mqtt_rrpc_response_fota_upgrade(ext, params);
    } else if (os_strcmp(svc_name, SVC_FOTA_CHECK) == 0) {
        aliot_mqtt_rrpc_response_fota_check(ext);
    } else if (os_strcmp(svc_name, SVC_GET_DEV_DATETIME) == 0) {
        aliot_mqtt_rrpc_response_datetime(ext);
    } else if (os_strcmp(svc_name, SVC_GET_PROPERTIES) == 0) {

    } else {
        aliot_mqtt_rrpc_response(ext, DEV_CODE_REQ_PARAM_ERR, "");
    }

    cJSON_Delete(root);
}
#endif
/***********************************RRPC End**************************************************/

/***********************************  COTA  **************************************************/
#ifdef  COTA_ENABLED
ICACHE_FLASH_ATTR void parse_cota_get_reply(const char *topic, const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return;
    };

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 200) {
        cJSON_Delete(root);
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!cJSON_IsObject(data)) {
        return;
    }

    cJSON *url = cJSON_GetObjectItem(data, "url");
    if (!cJSON_IsString(url)) {
        return;
    }

    LOGD(TAG, "url: %s", url->valuestring);
}

#define COTA_GET_PAYLOAD_FMT        "{\
\"id\":\"%s\",\
\"version\":\"1.0\",\
\"params\":{\
\"configScope\":\"product\",\
\"getType\":\"file\",\
},\
\"method\":\"thing.config.get\",\
}"
ICACHE_FLASH_ATTR void aliot_mqtt_cota_get() {
    int len = os_strlen(COTA_GET_PAYLOAD_FMT) + 10 + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed...");
        return;
    }
    os_snprintf(payload, len, COTA_GET_PAYLOAD_FMT, aliot_mqtt_getid());
    aliot_mqtt_publish(COTA_TOPIC_GET, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}
#endif
/***********************************COTA End**************************************************/

/**********************************************************************************************************/

ICACHE_FLASH_ATTR void aliot_mqtt_subscribe_topics() {
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

ICACHE_FLASH_ATTR void aliot_regist_fota_check_cb(bool (* callback)()) {
    aliot_callback.fota_check_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_fota_upgrade_cb(char* (* callback)(const cJSON *params)) {
    aliot_callback.fota_upgrade_cb = callback;
}

ICACHE_FLASH_ATTR void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time)) {
    aliot_callback.sntp_response_cb = callback;
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

//  ${msgid}    ${version}
#define FOTA_INFORM_PAYLOAD_FMT     "{\"id\":\"%s\",\"params\":{\"version\":\"%d\"}}"
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
    aliot_mqtt_publish(FOTA_TOPIC_INFORM, payload, 1, 0);
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
\"id\":\"%s\",\
\"version\":\"1.0\",\
\"params\":{%s},\
\"method\":\"thing.event.property.post\"\
}"
/**
 * @param params: 属性转换后的json格式字符串
 * */
ICACHE_FLASH_ATTR void aliot_mqtt_post_property(const char *id, const char *params) {
    int len = os_strlen(PROPERTY_POST_PAYLOAD_FMT) + os_strlen(params) + 10 + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        LOGD(TAG, "malloc payload failed.");
        return;
    }
    os_snprintf(payload, len, PROPERTY_POST_PAYLOAD_FMT, id, params);
    aliot_mqtt_publish(DEVMODEL_TOPIC_PROPERTY_POST, payload, 0, 0);
    os_free(payload);
    payload = NULL;
}

//  ${msgid}    ${params}
#define PROPERTY_HISTORY_POST_PAYLOAD_FMT   "{\"id\":\"%s\",\"version\":\"1.0\",\"params\":{\"properties\":[%s]},\"method\":\"thing.event.property.history.post\"}"
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
#define FOTA_PROGRESS_PAYLOAD_FMT   "{\"id\":\"%s\",\"params\":{\"step\":\"%d\",\"desc\":\"%s\"}}"   
ICACHE_FLASH_ATTR void aliot_mqtt_report_fota_progress(const int8_t step, const char *msg) {
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

/***********************************************************************************************/

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
        topic_str = (char *) os_zalloc(size);
        if (topic_str == NULL) {
            LOGE(TAG, "malloc topic_str failed...");
            continue;
        }
        os_snprintf(topic_str, size, ptopic->topic_fmt, meta->product_key, meta->device_name);
        bool match = false;

        // char *pwildchard = os_strchr(topic_str, '+');
        // if (pwildchard == NULL) {
        //     pwildchard = os_strchr(topic_str, '#');
        // }
        // if (pwildchard == NULL) {
        //     if (os_strcmp(topic, topic_str) == 0) {
        //         match = true;
        //         if (ptopic->parse_function != NULL) {
        //             ptopic->parse_function(payload);
        //         }
        //     }
        // } else {
        //     int prelen = pwildchard-topic_str;
        //     int suflen = os_strlen(pwildchard+1);
        //     if (os_strncmp(topic, topic_str, prelen) == 0 && os_strncmp(pwildchard+1, topic+os_strlen(topic)-suflen, suflen) == 0) {
        //         match = true;
        //         if (ptopic->parse_ext_function != NULL) {
        //             int len = os_strlen(topic)-prelen-suflen;
        //             pwildchard = (char *) os_zalloc(len+1);
        //             if (pwildchard == NULL) {
        //                 LOGE(TAG, "malloc pwildchard failed...");
        //             } else {
        //                 os_memcpy(pwildchard, topic+prelen, len);
        //                 LOGD(TAG, "wildchard: %s", pwildchard);
        //                 ptopic->parse_ext_function(pwildchard, payload);
        //                 os_free(pwildchard);
        //                 pwildchard = NULL;
        //             }
        //         }
        //     }
        // }

        if (os_strcmp(topic, topic_str) == 0) {
            match = true;
            if (ptopic->parse_function != NULL) {
                ptopic->parse_function(payload);
            }
        } else {
            char *pwildchard = os_strchr(topic_str, '+');
            if (pwildchard == NULL) {
                pwildchard = os_strchr(topic_str, '#');
            }
            if (pwildchard != NULL) {
                int prelen = pwildchard-topic_str;
                int suflen = os_strlen(pwildchard+1);
                if (os_strncmp(topic, topic_str, prelen) == 0 && os_strncmp(pwildchard+1, topic+os_strlen(topic)-suflen, suflen) == 0) {
                    match = true;
                    if (ptopic->parse_ext_function != NULL) {
                        int len = os_strlen(topic)-prelen-suflen;
                        pwildchard = (char *) os_zalloc(len+1);
                        if (pwildchard == NULL) {
                            LOGE(TAG, "malloc pwildchard failed...");
                        } else {
                            os_memcpy(pwildchard, topic+prelen, len);
                            LOGD(TAG, "wildchard: %s", pwildchard);
                            ptopic->parse_ext_function(pwildchard, payload);
                            os_free(pwildchard);
                            pwildchard = NULL;
                        }
                    }
                }
            }
        }

        os_free(topic_str);
        topic_str = NULL;
        if (match) {
            break;
        }
    }
}

// ICACHE_FLASH_ATTR static void aliot_mqtt_sync_sntp_fn(void *arg) {
//     check_conn_cnt++;
//     if (conn_sync_sntp || check_conn_cnt > SYNCT_SNTP_TIMEOUT) {
//         os_timer_disarm(&conncb_timer);
//         conn_sync_sntp = false;
//         check_conn_cnt = 0;
//         aliot_mqtt_report_version();
//         aliot_attr_post_all();
//     }

//     // if (conn_sync_sntp) {
//     //     os_timer_disarm(&conncb_timer);
//     //     conn_sync_sntp = false;
//     //     aliot_mqtt_report_version();
//     //     aliot_attr_post_all();
//     //     return;
//     // }
//     // check_conn_cnt++;
//     // if (check_conn_cnt > SYNCT_SNTP_TIMEOUT) {
//     //     os_timer_disarm(&conncb_timer);
//     //     check_conn_cnt = 0;
//     // }
// }

// ICACHE_FLASH_ATTR static void aliot_mqtt_conncb_delay(void *arg) {
//     conn_sync_sntp = false;
//     check_conn_cnt = 0;
//     os_timer_disarm(&conncb_timer);
//     os_timer_setfn(&conncb_timer, aliot_mqtt_sync_sntp_fn, NULL);
//     os_timer_arm(&conncb_timer, 1000, 1);
//     aliot_mqtt_get_sntptime();
// }

ICACHE_FLASH_ATTR void aliot_mqtt_connected_cb(uint32_t *args) {
    mqttConnected = true;
    MQTT_Client* client = (MQTT_Client*)args;
    LOGD(TAG, "Connected");

    aliot_mqtt_subscribe_topics();
    if (conn_sync_sntp == false) {
        aliot_mqtt_get_sntptime();
    }
    aliot_mqtt_report_version();
    aliot_attr_post_all();
    // aliot_mqtt_fota_reset();
    // aliot_mqtt_cota_get();

    // os_timer_disarm(&conncb_timer);
    // os_timer_setfn(&conncb_timer, aliot_mqtt_conncb_delay, NULL);
    // os_timer_arm(&conncb_timer, CONNECT_DELAY, 0);
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

    // MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
    MQTT_OnConnected(&mqttClient, aliot_mqtt_connected_cb);
    MQTT_OnDisconnected(&mqttClient, aliot_mqtt_disconnected_cb);
    MQTT_OnPublished(&mqttClient, aliot_mqtt_published_cb);
    MQTT_OnData(&mqttClient, aliot_mqtt_data_cb);

    MQTT_Connect(&mqttClient);
}
