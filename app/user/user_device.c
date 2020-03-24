#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"
#include "app_test.h"
#include "user_rtc.h"
#include "udpserver.h"
#include "wrappers_product.h"

static const char *TAG = "Device";

#define	ZONE				"zone"

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

ICACHE_FLASH_ATTR void user_device_process(void *arg) {
	user_device_t *pdev = arg;
	if (pdev == NULL || pdev->process == NULL) {
		return;
	}
	// sint8_t rssi = wifi_station_get_rssi();
	if (user_rtc_is_synchronized()) {
		uint64_t current_time = user_rtc_get_time();
		uint32_t v1 = current_time/100000000;
		uint32_t v2 = current_time%100000000;
		os_memset(pdev->device_time, 0, sizeof(pdev->device_time));
		os_sprintf(pdev->device_time, "%d%d", v1, v2);
		pdev->attrDeviceTime.changed = true;
	}

	pdev->process(arg);
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
	char mac[6];
	wifi_get_macaddr(SOFTAP_IF, mac);
	os_sprintf(p_user_dev->apssid, "%s_%02X%02X%02X", p_user_dev->product, mac[3], mac[4], mac[5]);

	if (user_device_poweron_check()) {
		system_restore();
		app_test_init(p_user_dev);
	} else {
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
	buf[0] = '{';
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
		LOGD(TAG, "response: %s", buf);
		udpserver_send(buf, len);
	}
	os_free(buf);
	buf = NULL;
}

ICACHE_FLASH_ATTR static void parse_udprcv_set(cJSON *request) {
	if (cJSON_IsObject(request) == false) {
		return;
	}
	bool result = false;
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
	if (cJSON_IsNumber(zone)) {
		if (p_user_dev != NULL && p_user_dev->setzone != NULL) {
			p_user_dev->setzone(zone->valueint);
		}
	}
	if (result) {
		os_delay_us(50000);
		system_restart();
	}
}

ICACHE_FLASH_ATTR static void user_device_parse_udp_rcv(const char *buf) {
	LOGD(TAG, "udprcv: %s", buf);
	cJSON *root = cJSON_Parse(buf);
	if (cJSON_IsObject(root) == false) {
		cJSON_Delete(root);
		return;
	}
	if (cJSON_HasObjectItem(root, "GET")) {
		cJSON *getReq = cJSON_GetObjectItem(root, "GET");
		parse_udprcv_get(getReq);
	} else if (cJSON_HasObjectItem(root, "SET")) {
		cJSON *setReq = cJSON_GetObjectItem(root, "SET");
		parse_udprcv_set(setReq);
	}
	cJSON_Delete(root);
}