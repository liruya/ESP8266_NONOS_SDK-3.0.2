#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"
#include "app_test.h"
#include "user_rtc.h"

static const char *TAG = "Device";

#define	PSW_ENABLE_MASK		0xFF000000
#define	PSW_ENABLE_FLAG		0x55000000
#define	PSW_MASK			0x00FFFFFF
#define	PSW_MAX				999999

static os_timer_t proc_timer;

// bool ICACHE_FLASH_ATTR user_device_psw_isvalid(user_device_t *pdev) {
// 	if (pdev == NULL || pdev->pconfig == NULL) {
// 		return false;
// 	}
// 	if ((pdev->pconfig->local_psw&PSW_ENABLE_MASK) == PSW_ENABLE_FLAG && (pdev->pconfig->local_psw&PSW_MASK) <= PSW_MAX) {
// 		return true;
// 	}
// 	return false;
// }

ICACHE_FLASH_ATTR bool user_device_poweron_check(user_device_t *pdev) {
	if (pdev == NULL) {
		return false;
	}
	bool flag = false;
	uint8_t cnt = 0;

	while(cnt < 250) {
		cnt++;
		os_delay_us(10000);
		system_soft_wdt_feed();
		if (GPIO_INPUT_GET(pdev->key_io_num) == 0) {
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
			if(GPIO_INPUT_GET(pdev->key_io_num) == 0) {
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

ICACHE_FLASH_ATTR void user_device_board_init(user_device_t *pdev) {
	if (pdev == NULL || pdev->board_init == NULL) {
		LOGE(TAG, "device board init failed...");
		return;
	}
	pdev->board_init();
}

ICACHE_FLASH_ATTR void user_device_init(user_device_t *pdev) {
	if (pdev == NULL || pdev->init == NULL) {
		LOGE(TAG, "device init failed...");
		return;
	}
	char mac[6];
	wifi_get_macaddr(SOFTAP_IF, mac);
	os_sprintf(pdev->apssid, "%s_%02X%02X%02X", pdev->product, mac[3], mac[4], mac[5]);

	if (user_device_poweron_check(pdev)) {
		system_restore();
		app_test_init(pdev);
	} else {
		pdev->init();
	}

	os_timer_disarm(&proc_timer);
	os_timer_setfn(&proc_timer, user_device_process, pdev);
	os_timer_arm(&proc_timer, 1000, 1);
}
