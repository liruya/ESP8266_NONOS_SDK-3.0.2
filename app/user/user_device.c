#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"
#include "app_test.h"
#include "user_rtc.h"
#include "udpserver.h"
#include "ota.h"
#include "cJSON.h"
#include "dev_sign.h"
#include "aliot_sign.h"
#include "dynreg.h"

static const char *TAG = "Device";

#define	SRVC_GET_DATETIME		"get_device_datetime"
#define	SRVC_FOTA_CHECK			"fota_check"
#define	SRVC_FOTA_UPGRADE		"fota_upgrade"

#define	ZONE					"zone"
#define	TIME					"time"
#define	DEVICE_DATETIME			"device_datetime"

#define	PSW_ENABLE_MASK			(0xFF000000)
#define	PSW_ENABLE_FLAG			(0x55000000)
#define	PSW_MASK				(0x00FFFFFF)
#define	PSW_MAX					(999999)

#define	RTC_SYNC_INTERVAL		((uint64_t) 2*60*60*1000)		// msec, 2hours

static cJSON* getDeviceInfoJson(aliot_attr_t *attr);
static void user_device_parse_udp_rcv(const char *buf);
static void user_device_post_local(const char *params);

static os_timer_t proc_timer;

static user_device_t *pdev_instance;

const attr_vtable_t deviceInfoVtable = newReadAttrVtable(getDeviceInfoJson);

static bool wifi_connected;

static bool local_connected;
static os_timer_t udp_timer;

static os_timer_t conn_timer;

// bool ICACHE_FLASH_ATTR user_device_psw_isvalid(user_device_t *pdev) {
// 	if (pdev == NULL || pdev->pconfig == NULL) {
// 		return false;
// 	}
// 	if ((pdev->pconfig->local_psw&PSW_ENABLE_MASK) == PSW_ENABLE_FLAG && (pdev->pconfig->local_psw&PSW_MASK) <= PSW_MAX) {
// 		return true;
// 	}
// 	return false;
// }

ICACHE_FLASH_ATTR void  dynreg_success_cb(const char *dsecret) {
    if (user_device_set_device_secret(dsecret)) {
        aliot_mqtt_init(&pdev_instance->meta);
        aliot_mqtt_connect();
    }
}

ICACHE_FLASH_ATTR void  wifi_event_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED:
            LOGD(TAG, "start to connect to ssid %s, channel %d",
                evt->event_info.connected.ssid,
                evt->event_info.connected.channel);
				wifi_connected = true;
            if (pdev_instance != NULL) {
                os_memset(pdev_instance->dev_info.ssid, 0, sizeof(pdev_instance->dev_info.ssid));
                os_strcpy(pdev_instance->dev_info.ssid, evt->event_info.connected.ssid);
            }
            break;
        case EVENT_STAMODE_DISCONNECTED:
            LOGD(TAG, "disconnect from ssid %s, reason %d",
                evt->event_info.disconnected.ssid,
                evt->event_info.disconnected.reason);
                aliot_mqtt_disconnect();
				wifi_connected = false;
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            LOGD(TAG, "mode: %d -> %d",
                evt->event_info.auth_change.old_mode,
                evt->event_info.auth_change.new_mode);
            break;
        case EVENT_STAMODE_GOT_IP:
            LOGD(TAG, "ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "",
                IP2STR(&evt->event_info.got_ip.ip),
                IP2STR(&evt->event_info.got_ip.mask),
                IP2STR(&evt->event_info.got_ip.gw));
            if (pdev_instance != NULL) {
                os_memset(pdev_instance->dev_info.ipaddr, 0, sizeof(pdev_instance->dev_info.ipaddr));
                os_sprintf(pdev_instance->dev_info.ipaddr, IPSTR, IP2STR(&evt->event_info.got_ip.ip));
            }
            if (user_device_is_secret_valid()) {
                aliot_mqtt_connect();
            } else {
                dynreg_start(&pdev_instance->meta, dynreg_success_cb);
            }
            break;
        default:
            break;
    }
}

ICACHE_FLASH_ATTR bool user_device_poweron_check() {
	if (pdev_instance == NULL) {
		return false;
	}
	bool flag = false;
	uint8_t cnt = 0;

	while(cnt < 250) {
		cnt++;
		os_delay_us(10000);
		system_soft_wdt_feed();
		if (GPIO_INPUT_GET(pdev_instance->key_io_num) == 0) {
			flag = true;
			break;
		}
	}

	if (flag) {
		cnt = 0;
		//长按4秒
		while(cnt < 200) {
			system_soft_wdt_feed();
			os_delay_us(20000);
			if(GPIO_INPUT_GET(pdev_instance->key_io_num) == 0) {
				cnt++;
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR char* user_device_get(const char *key) {
	if (pdev_instance == NULL || key == NULL) {
		return NULL;
	}
	if (strcmp(key, REGION) == 0) {
		return (char *) pdev_instance->meta.region;
	}
	if (strcmp(key, PRODUCT_KEY) == 0) {
		return (char *) pdev_instance->meta.product_key;
	}
	if (strcmp(key, PRODUCT_SECRET) == 0) {
		return (char *) pdev_instance->meta.product_secret;
	}
	if (strcmp(key, DEVICE_NAME) == 0) {
		return pdev_instance->meta.device_name;
	}
	if (strcmp(key, DEVICE_SECRET) == 0) {
		return pdev_instance->meta.device_secret;
	}
	if (strcmp(key, MAC_ADDRESS) == 0) {
		return pdev_instance->mac;
	}
	if (strcmp(key, DEVICE_DATETIME) == 0) {
		return pdev_instance->device_time;
	}
	return NULL;
}

ICACHE_FLASH_ATTR bool user_device_is_secret_valid() {
	if (pdev_instance == NULL) {
		return false;
	}
	char c;
	for (int i = 0; i < DEVICE_SECRET_LEN; i++) {
		c = pdev_instance->meta.device_secret[i];
		if (c == '\0') {
			if (i == 0) {
				return false;
			} else {
				return true;
			}
		} else if (c >= '0' && c <= '9') {

		} else if (c >= 'a' && c <= 'z') {

		} else if (c >= 'A' && c <= 'Z') {

		} else {
			return false;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR void user_device_get_device_secret() {
	if (pdev_instance == NULL) {
		return;
	}
	// system_param_load(ALIOT_CONFIG_SECTOR, DEVICE_SECRET_OFFSET, pdev_instance->meta.device_secret, sizeof(pdev_instance->meta.device_secret));
	os_memset(pdev_instance->meta.device_secret, 0, sizeof(pdev_instance->meta.device_secret));
	system_param_load(ALIOT_CONFIG_SECTOR, 96, pdev_instance->meta.device_secret, DEVICE_SECRET_LEN);
	if (user_device_is_secret_valid()) {
		system_param_save_with_protect(ALIOT_CONFIG_SECTOR, pdev_instance->meta.device_secret, sizeof(pdev_instance->meta.device_secret));
	} else {
		system_param_load(ALIOT_CONFIG_SECTOR, DEVICE_SECRET_OFFSET, pdev_instance->meta.device_secret, DEVICE_SECRET_LEN);
		if (user_device_is_secret_valid() == false) {
			os_memset(pdev_instance->meta.device_secret, 0, sizeof(pdev_instance->meta.device_secret));
		}
	}
}

ICACHE_FLASH_ATTR bool user_device_set_device_secret(const char *device_secret) {
	if (pdev_instance == NULL || device_secret == NULL || os_strlen(device_secret) > DEVICE_SECRET_LEN) {
		return false;
	}
	os_memset(pdev_instance->meta.device_secret, 0, sizeof(pdev_instance->meta.device_secret));
	os_strcpy(pdev_instance->meta.device_secret, device_secret);
	if (user_device_is_secret_valid()) {
		return system_param_save_with_protect(ALIOT_CONFIG_SECTOR, pdev_instance->meta.device_secret, sizeof(pdev_instance->meta.device_secret));
	}
	return false;
}

ICACHE_FLASH_ATTR static void user_device_sync_time_cb(void *arg) {
	os_timer_t *timer = (os_timer_t *) arg;

	os_timer_disarm(timer);
	aliot_mqtt_get_sntptime(system_get_time());
	os_timer_arm(timer, RTC_SYNC_INTERVAL, 0);
}

ICACHE_FLASH_ATTR static bool user_device_sync_time() {
	static os_timer_t sync_timer;

	uint64_t sync_time = user_rtc_get_synchronized_time();
	if (sync_time == 0 || sync_time + RTC_SYNC_INTERVAL < user_rtc_get_time()) {
		aliot_mqtt_get_sntptime(system_get_time());
		os_timer_disarm(&sync_timer);
		os_timer_setfn(&sync_timer, user_device_sync_time_cb, &sync_timer);
		os_timer_arm(&sync_timer, RTC_SYNC_INTERVAL, 0);
	}
}

ICACHE_FLASH_ATTR static void user_device_report_version() {
	static bool reported = false;
	if (pdev_instance == NULL || reported) {
		return;
	}
	char version[16] = {0};
	os_sprintf(version, "%d", pdev_instance->firmware_version);
	aliot_mqtt_report_version(version);
	reported = true;
}

ICACHE_FLASH_ATTR static void user_device_subscribe_topics() {
	if (pdev_instance == NULL) {
		return;
	}
	char *topic_fmt[] = {
		TOPIC_SNTP_RESPONSE,
		TOPIC_CUSTOM_ALL
	};
	char topic[128] = {0};
	for (int i = 0; i < sizeof(topic_fmt)/sizeof(char *); i++) {
		os_memset(topic, 0, sizeof(topic));
		os_sprintf(topic, topic_fmt[i], pdev_instance->meta.product_key, pdev_instance->meta.device_name);
		aliot_mqtt_subscribe_topic(topic, 0);
	}
}

/* publish 		{productKey} {deviceName} */
#define	TOPIC_FOTA_PROGRESS_FMT		"/%s/%s/user/fota/progress"
#define FOTA_PROGRESS_PAYLOAD_FMT   "{\"params\":{\"step\":\"%d\",\"desc\":\"%s\"}}"  
ICACHE_FLASH_ATTR static void user_device_fota_report_progress(const int8_t step, const char *desc) {
	uint32_t len = os_strlen(FOTA_PROGRESS_PAYLOAD_FMT) + 4 + os_strlen(desc) + 1;
	char *payload = (char *) os_zalloc(len);
	if (payload == NULL) {
		LOGE(TAG, "zalloc payload failed");
		return;
	}
	os_memset(payload, 0, len);
	os_sprintf(payload, FOTA_PROGRESS_PAYLOAD_FMT, step, desc);

	aliot_mqtt_publish(TOPIC_FOTA_PROGRESS_FMT, payload, 0);
	os_free(payload);
	payload	= NULL;
}

#define DEVICE_DATETIME_FMT "{\"device_datetime\":\"%s\"}"
static void user_device_get_datetime(const char *rrpcid, const char *msgid, cJSON *params) {
	char payload[64] = {0};
	os_sprintf(payload, DEVICE_DATETIME_FMT, pdev_instance->device_time);
	aliot_mqtt_sync_service_reply(rrpcid, msgid, 200, payload);
}

#define FOTA_CHECK_RESPONSE_FMT     "{\"version\":%d,\"upgrading\":%d}"
static void user_device_fota_check(const char *rrpcid, const char *msgid, cJSON *params) {
	char payload[64] = {0};
	os_sprintf(payload, FOTA_CHECK_RESPONSE_FMT, pdev_instance->firmware_version, ota_is_upgrading());
	aliot_mqtt_sync_service_reply(rrpcid, msgid, 200, payload);
}

#define	FOTA_RESULT_FMT			"{\"code\":%d,\"message\":\"%s\"}"
ICACHE_FLASH_ATTR void user_device_fota_upgrade(const char *rrpcid, const char *msgid, cJSON *params) {
	do {
		cJSON *curl = cJSON_GetObjectItem(params, "url");
		if (cJSON_IsString(curl) == false) {
			break;
		}
		const char *url = curl->valuestring;

		cJSON *cversion = cJSON_GetObjectItem(params, "version");
		if (cJSON_IsNumber(cversion) == false || cversion->valueint <= 0 || cversion->valueint > 0xFFFF) {
			break;
		}
		uint16_t version = cversion->valueint;

		uint16_t curr_version = pdev_instance->firmware_version;
		int code = UERR_SUCCESS;
		char *msg = "";
		if (version <= curr_version) {
			code = UERR_FOTA_UPTODATE;
		} else if (((version-curr_version)&0x01) == 0) {
			code = UERR_FOTA_VERSION;
		} else {
			code = ota_start(url);
		}
		switch (code) {
			case UERR_FOTA_UPGRADING: {
				msg = UMSG_FOTA_UPGRADING;
			} break;
			case UERR_FOTA_PARAMS: {
				msg = UMSG_FOTA_PARAMS;
			} break;
			case UERR_FOTA_URL: {
				msg = UMSG_FOTA_URL;
			} break;
			case UERR_FOTA_VERSION: {
				msg = UMSG_FOTA_VERSION;
			} break;
			case UERR_FOTA_UPTODATE: {
				msg = UMSG_FOTA_UPTODATE;
			} break;
			default: {

			} break;
		}
		char payload[128] = {0};
		os_sprintf(payload, FOTA_RESULT_FMT, code, msg);
		aliot_mqtt_sync_service_reply(rrpcid, msgid, 200, payload);
		return;
	} while (0);
	aliot_mqtt_sync_service_reply(rrpcid, msgid, 460, "{}");
}

ICACHE_FLASH_ATTR static void user_device_post_all() {
	cJSON *params = aliot_attr_get_json(false);
	if (params == NULL) {
		return;
	}
	char *payload = cJSON_PrintUnformatted(params);
	aliot_mqtt_post_property(aliot_mqtt_getid(), payload);
	cJSON_Delete(params);
}

ICACHE_FLASH_ATTR void user_device_post_changed() {
	cJSON *params = aliot_attr_get_json(true);
	if (params == NULL) {
		return;
	}
	char *payload = cJSON_PrintUnformatted(params);
	if (local_connected) {
		user_device_post_local(payload);
	}
	aliot_mqtt_post_property(aliot_mqtt_getid(), payload);
	cJSON_Delete(params);
}

ICACHE_FLASH_ATTR static void user_device_mqtt_connected_cb() {
	user_device_subscribe_topics();

	user_device_sync_time();

	user_device_report_version();

	user_device_post_all();
}

ICACHE_FLASH_ATTR static void user_device_property_set_cb(const char *msgid, cJSON *params) {
	bool ret = aliot_attr_parse_set(params);
	if (ret) {
		if (pdev_instance->attr_set_cb != NULL) {
			pdev_instance->attr_set_cb();
		}
		user_device_post_changed();
	}
}

ICACHE_FLASH_ATTR static void user_device_async_service_cb(const char *msgid, const char *service_id, cJSON *params) {

}

ICACHE_FLASH_ATTR static void user_device_sync_service_cb(const char *rrpcid, const char *msgid, const char *service_id, cJSON *params) {
	if (os_strcmp(service_id, SRVC_GET_DATETIME) == 0) {
		user_device_get_datetime(rrpcid, msgid, params);
	} else if (os_strcmp(service_id, SRVC_FOTA_CHECK) == 0) {
		user_device_fota_check(rrpcid, msgid, params);
	} else if (os_strcmp(service_id, SRVC_FOTA_UPGRADE) == 0) {
		user_device_fota_upgrade(rrpcid, msgid, params);
	}
}

ICACHE_FLASH_ATTR static void user_device_custom_cb(const char *custom_id, cJSON *params) {

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

	do {
		output[j++] = buf[--i];
	} while (i > 0);

	output[j] = '\0';
}

ICACHE_FLASH_ATTR void user_device_process(void *arg) {
	user_device_t *pdev = (user_device_t *) arg;

	system_soft_wdt_feed();

	if (wifi_connected) {
		pdev->dev_info.rssi = wifi_station_get_rssi();
		pdev->attrDeviceInfo.changed = true;
	} 
	if (user_rtc_is_synchronized()) {
		os_memset(pdev->device_time, 0, sizeof(pdev->device_time));
		const uint64_t time = user_rtc_get_time();
		uint64_to_str(time, pdev->device_time);
	}

	if (pdev->process != NULL) {
		pdev->process(arg);
	}
}

ICACHE_FLASH_ATTR void user_device_init(user_device_t *pdev) {
	if (pdev == NULL || pdev->board_init == NULL || pdev->init == NULL) {
		LOGE(TAG, "device init failed...");
		return;
	}
	pdev->board_init();

	wifi_set_event_handler_cb(wifi_event_cb);

	pdev_instance = pdev;
	uint8_t mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	os_sprintf(pdev_instance->mac, "%02X%02X%02X%02X%02X%02X", MAC2STR(mac));
	os_strcpy(pdev_instance->meta.device_name, pdev_instance->mac);
	os_sprintf(pdev_instance->dev_info.mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(mac));
	os_sprintf(pdev_instance->apssid, "%s_%02X%02X%02X", pdev_instance->product, mac[3], mac[4], mac[5]);
	user_device_get_device_secret();

    LOGD(TAG, "fw_version: %d", pdev_instance->firmware_version);
    LOGD(TAG, "region: %s", pdev_instance->meta.region);
    LOGD(TAG, "productKey: %s", pdev_instance->meta.product_key);
    LOGD(TAG, "productSecret: %s", pdev_instance->meta.product_secret);
    LOGD(TAG, "deviceName: %s", pdev_instance->meta.device_name);
	
	if (user_device_poweron_check()) {
		system_restore();
		app_test_init(pdev_instance);
	} else {
		struct softap_config config;
		if (wifi_get_opmode() == SOFTAP_MODE && wifi_softap_get_config_default(&config)) {
			if (os_strcmp(config.ssid, pdev_instance->apssid) != 0) {
				os_memset(config.ssid, 0, sizeof(config.ssid));
				os_strcpy(config.ssid, pdev_instance->apssid);
				config.ssid_len = os_strlen(config.ssid);
				wifi_softap_set_config(&config);
			}
		}
		pdev_instance->init();
	}
	udpserver_init(user_device_parse_udp_rcv);

	os_timer_disarm(&proc_timer);
	os_timer_setfn(&proc_timer, user_device_process, pdev_instance);
	os_timer_arm(&proc_timer, 1000, 1);

	if (user_device_is_secret_valid()) {
    	LOGD(TAG, "deviceSecret: %s", pdev_instance->meta.device_secret);
        aliot_mqtt_init(&pdev_instance->meta);
    }
	aliot_regist_mqtt_connect_cb(user_device_mqtt_connected_cb);

	aliot_regist_sntp_response_cb(user_rtc_set_time);
	aliot_regist_property_set_cb(user_device_property_set_cb);
	aliot_regist_async_service_cb(user_device_async_service_cb);
	aliot_regist_sync_service_cb(user_device_sync_service_cb);
	aliot_regist_custom_cb(user_device_custom_cb);

	ota_regist_progress_cb(user_device_fota_report_progress);
}

static cJSON* getDeviceInfoJson(aliot_attr_t *attr) {
	device_info_t *pinfo = (device_info_t *) attr->attrValue;
	cJSON *result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "mac", pinfo->mac);
	cJSON_AddStringToObject(result, "ssid", pinfo->ssid);
	cJSON_AddStringToObject(result, "ip", pinfo->ipaddr);
	cJSON_AddNumberToObject(result, "rssi", pinfo->rssi);
	return result;
}

/* ****************************************************************************
 * 
 * parse udpserver receive data
 * 
 * ***************************************************************************/

ICACHE_FLASH_ATTR static void parse_udprcv_get(cJSON *request) {
	if (cJSON_IsArray(request) == false) {
		return;
	}
	int size = cJSON_GetArraySize(request);
	if (size <= 0) {
		return;
	}
	cJSON *result = cJSON_CreateObject();
	cJSON *resp = cJSON_CreateObject();
	for (int i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(item)) {
			char *key = item->valuestring;
			char *value = user_device_get(key);
			if (key != NULL && value != NULL) {
				cJSON_AddStringToObject(resp, key, value);
			}
		}
	}
    cJSON_AddItemToObject(result, "get_resp", resp);
    char *payload = cJSON_PrintUnformatted(result);
    udpserver_send(payload, os_strlen(payload));
}

ICACHE_FLASH_ATTR void conn_timer_cb(void *arg) {
	user_device_get_device_secret();
	if (user_device_is_secret_valid()) {
		aliot_mqtt_init(&pdev_instance->meta);
		aliot_mqtt_connect();
	}
}

#define SET_SUCCESS_RESPONSE    "{\"set_resp\":{\"result\":\"success\"}}"
ICACHE_FLASH_ATTR static void parse_udprcv_set(cJSON *request) {
	if (cJSON_IsObject(request) == false) {
		return;
	}
	bool result = false;
	cJSON *time = cJSON_GetObjectItem(request, TIME);
	if (cJSON_IsNumber(time) == false) {
		return;
	}
	cJSON *zone = cJSON_GetObjectItem(request, ZONE);
	if (cJSON_IsNumber(zone) == false) {
		return;
	}
	if (cJSON_HasObjectItem(request, DEVICE_SECRET)) {
		cJSON *dsecret = cJSON_GetObjectItem(request, DEVICE_SECRET);
		if (cJSON_IsString(dsecret) == false) {
			return;
		}
		if (user_device_set_device_secret(dsecret->valuestring) == false) {
			return;
		}
		result = true;
	}
	
	aliot_attr_t *attr = &pdev_instance->attrZone;
	if (attr->vtable->parseResult(attr, zone) == false) {
		return;
	}
	user_rtc_set_time((uint64_t) time->valuedouble);
	attr->changed = true;
	pdev_instance->attr_set_cb();

	udpserver_send(SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));
	os_delay_us(50000);
	udpserver_send(SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));

	if (result) {
		aliot_mqtt_deinit();
		
		os_timer_disarm(&conn_timer);
		os_timer_setfn(&conn_timer, conn_timer_cb, NULL);
		os_timer_arm(&conn_timer, 1000, 0);
	}
}

#define	SCAN_RESP_FMT	"{\"scan_resp\":{\
\"version\":%d,\
\"productKey\":\"%s\",\
\"deviceName\":\"%s\",\
\"mac\":\"%s\"}\
}"
ICACHE_FLASH_ATTR static void parse_udprcv_scan(cJSON *request) {
	if (pdev_instance == NULL) {
		return;
	}
	if (cJSON_IsArray(request) == false) {
		return;
	}
	bool match = false;
	int size = cJSON_GetArraySize(request);
	for (int i = 0; i < size; i++) {
		cJSON *pkey = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(pkey) && os_strcmp(pkey->valuestring, pdev_instance->meta.product_key) == 0) {
			match = true;
			break;
		}
	}
	if (match) {
		int len = os_strlen(SCAN_RESP_FMT) + PRODUCT_KEY_LEN + DEVICE_NAME_LEN + 24;
		char *payload = os_zalloc(len);
		if (payload == NULL) {
			LOGE(TAG, "malloc payload failed.");
			return;
		}
		os_sprintf(payload, SCAN_RESP_FMT, pdev_instance->firmware_version, pdev_instance->meta.product_key, pdev_instance->meta.device_name, pdev_instance->mac);
		uint8_t delay;
		os_get_random(&delay, 1);
		os_delay_us(delay*200);
		udpserver_send(payload, os_strlen(payload));
		os_free(payload);
		payload = NULL;
	}
}

#define PROPERTY_LOCAL_POST_PAYLOAD_FMT   "{\
\"productKey\":\"%s\",\
\"deviceName\":\"%s\",\
\"params\":%s\
}"
ICACHE_FLASH_ATTR static void user_device_post_local(const char *params) {
	uint32_t size = strlen(PROPERTY_LOCAL_POST_PAYLOAD_FMT) + PRODUCT_KEY_LEN + DEVICE_NAME_LEN + strlen(params) + 1;
	char *payload = (char *) os_zalloc(size);
	if (payload == NULL) {
		LOGE(TAG, "zalloc buffer failed");
		return;
	}
	os_sprintf(payload, PROPERTY_LOCAL_POST_PAYLOAD_FMT, pdev_instance->meta.product_key, pdev_instance->meta.device_name, params);

	LOGD(TAG, "property post local: %s", payload);
	udpserver_send(payload, os_strlen(payload));
	os_free(payload);
	payload	= NULL;
}

ICACHE_FLASH_ATTR static void user_device_parse_properties_set(cJSON *request) {
	if (aliot_attr_parse_set(request)) {
		if (pdev_instance->attr_set_cb != NULL) {
			pdev_instance->attr_set_cb();
		}

		cJSON *params = aliot_attr_get_json(true);
		char *payload = cJSON_PrintUnformatted(params);
		user_device_post_local(payload);
		aliot_mqtt_post_property(aliot_mqtt_getid(), payload);

		cJSON_Delete(params);
	}
}

ICACHE_FLASH_ATTR static void user_device_parse_properties_get(cJSON *request) {
	cJSON *params = aliot_attr_parse_get_tojson(request);
	if (params == NULL) {
		return;
	}
	char *payload = cJSON_PrintUnformatted(params);
	LOGD(TAG, "property get post local: %s", payload);
	user_device_post_local(payload);
	cJSON_Delete(params);
}

ICACHE_FLASH_ATTR static void user_device_udp_timer_cb(void *arg) {
	local_connected = false;
}

#define	UDP_TIMEOUT				((uint32_t) 150*1000)
ICACHE_FLASH_ATTR static void user_device_parse_udp_rcv(const char *buf) {
	os_timer_disarm(&udp_timer);
	os_timer_setfn(&udp_timer, user_device_udp_timer_cb, NULL);
	local_connected = true;
	os_timer_arm(&udp_timer, UDP_TIMEOUT, 0);

	LOGD(TAG, "udprcv: %s", buf);
	cJSON *root = cJSON_Parse(buf);
	if (cJSON_IsObject(root) == false) {
		cJSON_Delete(root);
		return;
	}
	cJSON *request = NULL;
	if (cJSON_HasObjectItem(root, "get")) {
		request = cJSON_GetObjectItem(root, "get");
		parse_udprcv_get(request);
	} else if (cJSON_HasObjectItem(root, "set")) {
		request = cJSON_GetObjectItem(root, "set");
		parse_udprcv_set(request);
	} else if (cJSON_HasObjectItem(root, "scan")) {
		request = cJSON_GetObjectItem(root, "scan");
		parse_udprcv_scan(request);
	} else if (cJSON_HasObjectItem(root, "property")) {
		cJSON *property = cJSON_GetObjectItem(root, "property");
		if (cJSON_IsObject(property)) {
			cJSON *pkey = cJSON_GetObjectItem(property, "productKey");
			cJSON *dname = cJSON_GetObjectItem(property, "deviceName");
			request = cJSON_GetObjectItem(property, "params");
			if (!cJSON_IsString(pkey) || os_strcmp(pkey->valuestring, pdev_instance->meta.product_key) != 0
				|| !cJSON_IsString(dname) || os_strcmp(dname->valuestring, pdev_instance->meta.device_name) != 0) {
				cJSON_Delete(root);
				return;
			}
			if (cJSON_IsObject(request)) {
				user_device_parse_properties_set(request);
			} else if (cJSON_IsArray(request)) {
				user_device_parse_properties_get(request);
			}
		}
	} 
	cJSON_Delete(root);
}
