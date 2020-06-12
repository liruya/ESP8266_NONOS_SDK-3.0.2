#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"
#include "app_test.h"
#include "user_rtc.h"
#include "udpserver.h"
#include "wrappers_product.h"

static const char *TAG = "Device";

#define	ZONE				"zone"
#define	TIME				"time"

#define	PSW_ENABLE_MASK		0xFF000000
#define	PSW_ENABLE_FLAG		0x55000000
#define	PSW_MASK			0x00FFFFFF
#define	PSW_MAX				999999

static void user_device_parse_udp_rcv(const char *buf);

static os_timer_t proc_timer;

static user_device_t *p_user_dev;

// bool ICACHE_FLASH_ATTR user_device_psw_isvalid(user_device_t *pdev) {
// 	if (pdev == NULL || pdev->pconfig == NULL) {
// 		return false;
// 	}
// 	if ((pdev->pconfig->local_psw&PSW_ENABLE_MASK) == PSW_ENABLE_FLAG && (pdev->pconfig->local_psw&PSW_MASK) <= PSW_MAX) {
// 		return true;
// 	}
// 	return false;
// }

static os_timer_t restart_timer;
ICACHE_FLASH_ATTR static void system_restart_cb(void *arg) {
	system_restart();
}

ICACHE_FLASH_ATTR void system_restart_delay(uint32_t delay) {
	os_timer_disarm(&restart_timer);
	os_timer_setfn(&restart_timer, system_restart_cb, NULL);
	os_timer_arm(&restart_timer, delay, 0);
}

ICACHE_FLASH_ATTR bool user_device_poweron_check() {
	if (p_user_dev == NULL) {
		return false;
	}
	bool flag = false;
	uint8_t cnt = 0;

	while(cnt < 250) {
		cnt++;
		os_delay_us(10000);
		system_soft_wdt_feed();
		if (GPIO_INPUT_GET(p_user_dev->key_io_num) == 0) {
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
			if(GPIO_INPUT_GET(p_user_dev->key_io_num) == 0) {
				cnt++;
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR int user_device_get_version() {
	if (p_user_dev != NULL) {
		return p_user_dev->firmware_version;
	}
	return 0;
}

ICACHE_FLASH_ATTR void user_device_process(void *arg) {
	user_device_t *pdev = arg;
	if (pdev == NULL) {
		return;
	}
	pdev->dev_info.rssi = wifi_station_get_rssi();
	if (user_rtc_is_synchronized()) {
		uint64_t current_time = user_rtc_get_time();
		uint32_t v1 = current_time/100000000;
		uint32_t v2 = current_time%100000000;
		os_memset(pdev->device_time, 0, sizeof(pdev->device_time));
		os_sprintf(pdev->device_time, "%d%08d", v1, v2);
		// pdev->attrDeviceTime.changed = true;
	}
	system_soft_wdt_feed();

	if (pdev->process != NULL) {
		pdev->process(arg);
	}
}

ICACHE_FLASH_ATTR void user_device_board_init() {
	if (p_user_dev == NULL || p_user_dev->board_init == NULL) {
		LOGE(TAG, "device board init failed...");
		return;
	}
	p_user_dev->board_init();
}

ICACHE_FLASH_ATTR void user_device_init() {
	if (p_user_dev == NULL || p_user_dev->init == NULL) {
		LOGE(TAG, "device init failed...");
		return;
	}
	uint8_t mac[6];
	wifi_get_macaddr(STATION_IF, mac);
	os_sprintf(p_user_dev->dev_info.mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	os_sprintf(p_user_dev->apssid, "%s_%02X%02X%02X", p_user_dev->product, mac[3], mac[4], mac[5]);

	if (user_device_poweron_check()) {
		system_restore();
		app_test_init(p_user_dev);
	} else {
		struct softap_config config;
		if (wifi_get_opmode() == SOFTAP_MODE && wifi_softap_get_config_default(&config)) {
			if (os_strcmp(config.ssid, p_user_dev->apssid) != 0) {
				os_memset(config.ssid, 0, sizeof(config.ssid));
				os_strcpy(config.ssid, p_user_dev->apssid);
				config.ssid_len = os_strlen(config.ssid);
				wifi_softap_set_config(&config);
			}
		}
		p_user_dev->init();
	}
	udpserver_init(user_device_parse_udp_rcv);

	os_timer_disarm(&proc_timer);
	os_timer_setfn(&proc_timer, user_device_process, p_user_dev);
	os_timer_arm(&proc_timer, 1000, 1);
}

ICACHE_FLASH_ATTR void user_device_attch_instance(user_device_t *pdev) {
	if (pdev == NULL) {
		return;
	}
	p_user_dev = pdev;
}

#define	DEVICE_INFO_FMT		"\"%s\":{\
\"mac\":\"%s\",\
\"ssid\":\"%s\",\
\"ip\":\"%s\",\
\"rssi\":%d\
}"
ICACHE_FLASH_ATTR int getDeviceInfoString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	device_info_t *pinfo = (device_info_t *) attr->attrValue;
	os_sprintf(buf, DEVICE_INFO_FMT, attr->attrKey, pinfo->mac, pinfo->ssid, pinfo->ipaddr, pinfo->rssi);
	return os_strlen(buf);
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
	int i;
	char *buf = os_zalloc(256);
	if (buf == NULL) {
		return;
	}
	int cnt = 0;
	os_sprintf(buf, "{\"get_resp\":{");
	for (i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(item)) {
			char *key = item->valuestring;
			char *value = hal_product_get(key);
			if (key != NULL && value != NULL) {
				os_sprintf(buf+os_strlen(buf), "\"%s\":\"%s\",", key, value);
				cnt++;
			}
		}
	}
	if (cnt == 0) {
		buf[0] = '\0';
	} else {
		int len = os_strlen(buf);
		buf[len-1] = '}';
		buf[len] = '}';
		LOGD(TAG, "response: %s", buf);
		udpserver_send(buf, len+1);
	}
	os_free(buf);
	buf = NULL;
}

#define SET_SUCCESS_RESPONSE    "{\"set_resp\":{\"result\":\"success\"}}"
ICACHE_FLASH_ATTR static void parse_udprcv_set(cJSON *request) {
	if (cJSON_IsObject(request) == false) {
		return;
	}
	bool result = false;
	bool settime = false;
	cJSON *region = cJSON_GetObjectItem(request, REGION);
	if (cJSON_IsString(region)) {
		result |= hal_set_region(region->valuestring);
	}
	cJSON *pkey = cJSON_GetObjectItem(request, PRODUCT_KEY);
	if (cJSON_IsString(pkey)) {
		result |= hal_set_product_key(pkey->valuestring);
	}
	cJSON *psecret = cJSON_GetObjectItem(request, PRODUCT_SECRET);
	if (cJSON_IsString(psecret)) {
		result |= hal_set_product_secret(psecret->valuestring);
	}
	cJSON *dsecret = cJSON_GetObjectItem(request, DEVICE_SECRET);
	if (cJSON_IsString(dsecret)) {
		result |= hal_set_device_secret(dsecret->valuestring);
	}
	cJSON *zone = cJSON_GetObjectItem(request, ZONE);
	cJSON *time = cJSON_GetObjectItem(request, TIME);
	if (cJSON_IsNumber(zone) && cJSON_IsNumber(time)) {
		settime = true;
		if (p_user_dev != NULL && p_user_dev->settime != NULL) {
			p_user_dev->settime(zone->valueint, (uint64_t) time->valuedouble);
		}
	}

	if (result || settime) {
		udpserver_send(SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));
		os_delay_us(50000);
		udpserver_send(SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));
	}
	if (result) {
		system_restart_delay(2000);
	}
}

// static os_timer_t scan_timer;
// ICACHE_FLASH_ATTR void scan_resp_cb(void *arg) {
// 	char **parg = arg;
// 	char *payload = *parg;
// 	LOGD(TAG, "%d  %d  time send: %d  %s", parg, payload, system_get_time(), payload);
// 	udpserver_send(payload, os_strlen(payload));
// 	os_free(*parg);
// 	*parg = NULL;
// }

#define	SCAN_RESP_FMT	"{\"scan_resp\":{\
\"productKey\":\"%s\",\
\"deviceName\":\"%s\",\
\"mac\":\"%s\"}\
}"
ICACHE_FLASH_ATTR static void parse_udprcv_scan(cJSON *request) {
	if (p_user_dev == NULL) {
		return;
	}
	if (cJSON_IsArray(request) == false) {
		return;
	}
	int size = cJSON_GetArraySize(request);
	int i;
	for (i = 0; i < size; i++) {
		cJSON *pkey = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(pkey) && os_strcmp(pkey->valuestring, p_user_dev->productKey) == 0) {
			int len = os_strlen(SCAN_RESP_FMT) + PRODUCT_KEY_LEN + DEVICE_NAME_LEN + 24;
			char *payload = os_zalloc(len);
			if (payload == NULL) {
				LOGE(TAG, "malloc payload failed.");
				return;
			}
			os_sprintf(payload, SCAN_RESP_FMT, hal_product_get(PRODUCT_KEY), hal_product_get(DEVICE_NAME), hal_product_get(MAC_ADDRESS));
			uint8_t delay;
			os_get_random(&delay, 1);
			// os_timer_disarm(&scan_timer);
			// os_timer_setfn(&scan_timer, scan_resp_cb, &payload);
			// os_timer_arm(&scan_timer, delay+10, 0);
			// LOGD(TAG, "%d  %d  time: %d", &payload, payload, system_get_time());
			os_delay_us(delay*250);
			udpserver_send(payload, os_strlen(payload));
			os_free(payload);
			payload = NULL;
			return;
		}
	}
}

ICACHE_FLASH_ATTR static void user_device_parse_udp_rcv(const char *buf) {
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
	} else if (cJSON_HasObjectItem(root, "params")) {
		request = cJSON_GetObjectItem(root, "params");
		if (cJSON_IsObject(request)) {
			aliot_attr_parse_all(NULL, request, true);
		} else if (cJSON_IsArray(request)) {
			aliot_attr_parse_get(request, true);
		}
	}
	cJSON_Delete(root);
}
