#include "user_smartconfig.h"

typedef struct {
	void (*sc_pre_cb)();
	void (*sc_post_cb)();
} smartconfig_callback_t;

static const char *TAG = "SmartConfig";

static bool status;
static os_timer_t timer;
static smartconfig_callback_t sc_callback;

ICACHE_FLASH_ATTR static void user_smartconfig_done(sc_status status, void *pdata) {
	switch (status) {
		case SC_STATUS_WAIT:
			LOGD(TAG, "SC_STATUS_WAIT");
			break;
		case SC_STATUS_FIND_CHANNEL:
			LOGD(TAG, "SC_STATUS_FIND_CHANNEL");
			break;
		case SC_STATUS_GETTING_SSID_PSWD:
			LOGD(TAG, "SC_STATUS_GETTING_SSID_PSWD");
			sc_type *type = (sc_type *) pdata;
			if (*type == SC_TYPE_ESPTOUCH) {
				LOGD(TAG, "SC_TYPE:SC_TYPE_ESPTOUCH");
			} else {
				LOGD(TAG, "SC_TYPE:SC_TYPE_AIRKISS");
			}
			break;
		case SC_STATUS_LINK:
			LOGD(TAG, "SC_STATUS_LINK");
			wifi_set_opmode(STATION_MODE);
			wifi_station_disconnect();
			struct station_config *sta_conf = (struct station_config *) pdata;
			wifi_station_set_config(sta_conf);
			wifi_station_connect();
			break;
		case SC_STATUS_LINK_OVER:
			LOGD(TAG, "SC_STATUS_LINK_OVER");
			if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
				uint8 phone_ip[4] = { 0 };
				os_memcpy(phone_ip, (uint8*) pdata, 4);
				LOGD(TAG, "Phone ip: " IPSTR, IP2STR(phone_ip));
			} else {
				//SC_TYPE_AIRKISS - support airkiss v2.0
			}
			user_smartconfig_stop();
			break;
	}
}

ICACHE_FLASH_ATTR static void user_smartconfig_timeout_cb(void *arg) {
	user_smartconfig_stop();
}

ICACHE_FLASH_ATTR bool user_smartconfig_start(const uint32_t timeout, void (* pre_cb)(), void (* post_cb)()) {
	if (status) {
		return false;
	}
	status = true;
	sc_callback.sc_pre_cb = pre_cb;
	sc_callback.sc_post_cb = post_cb;
	if (sc_callback.sc_pre_cb != NULL) {
		sc_callback.sc_pre_cb();
	}

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, user_smartconfig_timeout_cb, NULL);
	os_timer_arm(&timer, timeout, 0);

	wifi_station_disconnect();
	smartconfig_stop();
	wifi_set_opmode_current(STATION_MODE);
	smartconfig_set_type(SC_TYPE_ESPTOUCH);
	smartconfig_start(user_smartconfig_done);
}

ICACHE_FLASH_ATTR void user_smartconfig_stop() {
	os_timer_disarm(&timer);
	smartconfig_stop();
	uint8_t mode = wifi_get_opmode_default();
	if (mode != STATION_MODE) {
		wifi_set_opmode_current(mode);
	} else {
		wifi_station_connect();
	}
	status = false;
	if (sc_callback.sc_post_cb != NULL) {
		sc_callback.sc_post_cb();
	}
}

ICACHE_FLASH_ATTR bool user_smartconfig_status() {
	return status;
}