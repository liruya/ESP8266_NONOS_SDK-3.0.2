#include "app_test.h"

#define	TEST_FLASH_PERIOD		500
#define	TEST_FLASH_COUNT		4

#define	TEST_SSID				"TP-LINK_E152"
#define	TEST_PSW				"inledco152"

typedef struct {
	uint32_t count;
	os_timer_t led_timer;
} test_para_t;

static const char *TAG = "Test";

static test_para_t *ptest = NULL;
static bool test_status;

ICACHE_FLASH_ATTR void app_test_flash_cb(void *arg) {
	if (ptest == NULL) {
		return;
	}
	user_device_t *pdev = arg;
	gpio_toggle(pdev->test_led1_num);
	gpio_toggle(pdev->test_led2_num);
	ptest->count++;
	if (ptest->count > TEST_FLASH_COUNT*2) {
		os_timer_disarm(&ptest->led_timer);
		os_free(ptest);
		ptest = NULL;
		
		gpio_high(pdev->test_led1_num);
		gpio_high(pdev->test_led2_num);
		user_device_init(pdev);
	}
}

ICACHE_FLASH_ATTR bool app_test_status() {
	return test_status;
}

ICACHE_FLASH_ATTR void app_test_init(user_device_t *pdev) {
	if (pdev == NULL) {
		return;
	}
	struct station_config config;
	os_memset(&config, 0, sizeof(config));
	os_strcpy(config.ssid, TEST_SSID);
	os_strcpy(config.password, TEST_PSW);
	config.threshold.authmode = AUTH_WPA_WPA2_PSK;
	wifi_set_opmode_current(STATION_MODE);
    wifi_station_set_config_current(&config);
	wifi_station_connect();
	
	gpio_low(pdev->test_led1_num);
	gpio_high(pdev->test_led2_num);

	ptest = (test_para_t *) os_zalloc(sizeof(test_para_t));
	if (ptest == NULL) {
		LOGE(TAG, "test init failed -> malloc failed");
		return;
	}
	os_timer_disarm(&ptest->led_timer);
	os_timer_setfn(&ptest->led_timer, app_test_flash_cb, pdev);
	os_timer_arm(&ptest->led_timer, TEST_FLASH_PERIOD, 1);
	test_status = true;
}
