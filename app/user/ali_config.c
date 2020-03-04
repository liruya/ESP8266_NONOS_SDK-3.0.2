#include "ali_config.h"
#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "cJSON.h"
#include "sntp.h"
#include "ota.h"
#include "aliot_mqtt.h"
#include "stdlib.h"

void parse_fota_upgrade(const char *payload);
void parse_devlabel_update_reply(const char *payload);
void parse_devlabel_delete_reply(const char *payload);
void parse_sntp_response(const char *payload);
void parse_property_post_reply(const char *payload) ;
void parse_property_set(const char *payload);
void parse_custom_get(const char *payload);

subscribe_topic_t ALIOT_SUBSCRIBE_TOPICS[7] = {
	{
		.topic_fmt 		= FOTA_TOPIC_UPGRADE,
		.qos			= 0,
		.parse_function	= parse_fota_upgrade
	},

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

	{
		.topic_fmt 		= SNTP_TOPIC_RESPONSE,
		.qos			= 0,
		.parse_function	= parse_sntp_response
	},

	{
		.topic_fmt 		= DEVMODEL_PROPERTY_TOPIC_POST_REPLY,
		.qos			= 0,
		.parse_function	= parse_property_post_reply
	},

	{
		.topic_fmt 		= DEVMODEL_PROPERTY_TOPIC_SET,
		.qos			= 0,
		.parse_function	= parse_property_set
	},

	{
		.topic_fmt 		= CUSTOM_TOPIC_GET,
		.qos			= 0,
		.parse_function	= parse_custom_get
	}
};

void ICACHE_FLASH_ATTR parse_fota_upgrade(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        return;
    }
    if (cJSON_IsObject(root) == false) {
        cJSON_Delete(root);
        return;
    }
    cJSON *msg = cJSON_GetObjectItem(root, "message");
    if (msg == NULL || cJSON_IsString(msg) == false || os_strcmp(msg->valuestring, "success") != 0) {
        cJSON_Delete(root);
        return;
    }
    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL || cJSON_IsObject(data) == false) {
        cJSON_Delete(root);
        return;
    }
    cJSON *size = cJSON_GetObjectItem(data, "size");
    cJSON *version = cJSON_GetObjectItem(data, "version");
    if (version == NULL || cJSON_IsString(version) == false) {
        cJSON_Delete(root);
        return;
    }
    cJSON *url = cJSON_GetObjectItem(data, "url");
    if (url == NULL || cJSON_IsString(url) == false) {
        cJSON_Delete(root);
        return;
    }
    os_printf("ota size: %d\n", (uint32_t) size->valuedouble);
    os_printf("ota version: %s\n", version->valuestring);
    os_printf("ota url: %s\n", url->valuestring);
    ota_start(version->valuestring, url->valuestring, aliot_mqtt_report_fota_progress);
    cJSON_Delete(root);
}

void ICACHE_FLASH_ATTR parse_devlabel_update_reply(const char *payload) {
    
}

void ICACHE_FLASH_ATTR parse_devlabel_delete_reply(const char *payload) {

}

void ICACHE_FLASH_ATTR parse_sntp_response(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL || !cJSON_IsObject(root)) {
        return;
    }
    cJSON *deviceSend = cJSON_GetObjectItem(root, "deviceSendTime");
    if (deviceSend == NULL || !cJSON_IsNumber(deviceSend)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *serverRecv = cJSON_GetObjectItem(root, "serverRecvTime");
    if (serverRecv == NULL || !cJSON_IsNumber(serverRecv)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *serverSend = cJSON_GetObjectItem(root, "serverSendTime");
    if (serverSend == NULL || !cJSON_IsNumber(serverSend)) {
        cJSON_Delete(root);
        return;
    }
    uint64_t devSendTime = deviceSend->valuedouble;
    uint64_t servRecvTime = serverRecv->valuedouble;
    uint64_t servSendTime = serverSend->valuedouble;
    uint64_t devRecvTime = system_get_time();
    os_printf("devSend: %lld devRecv %lld    servRecv: %lld servSend: %lld\n", 
                devSendTime, devRecvTime, servRecvTime, servSendTime);
    uint64_t result = (servRecvTime + servSendTime + ((0xFFFFFFFF-devSendTime+devRecvTime+1)&0xFFFFFFFF)/1000)/2;
    os_printf("current time: %lld    %s\n", result, sntp_get_real_time(result/1000));
    cJSON_Delete(root);
}

void ICACHE_FLASH_ATTR parse_property_post_reply(const char *payload) {

}

void ICACHE_FLASH_ATTR parse_property_set(const char *payload) {
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL || !cJSON_IsObject(root)) {
        return;
    }
    cJSON *params = cJSON_GetObjectItem(root, "params");
    if (params == NULL || !cJSON_IsObject(params)) {
        cJSON_Delete(root);
        return;
    }
    cJSON *powerSwitch = cJSON_GetObjectItem(params, "PowerSwitch");
    if (powerSwitch == NULL || !cJSON_IsNumber(powerSwitch)) {
        cJSON_Delete(root);
        return;
    }
    bool result = powerSwitch->valuedouble;
    os_printf("PowerSwitch set: %d\n", result);
    cJSON_Delete(root);
}

void ICACHE_FLASH_ATTR parse_custom_get(const char *payload) {

}
