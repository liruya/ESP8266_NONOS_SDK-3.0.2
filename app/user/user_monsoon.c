#include "user_monsoon.h"
#include "app_board_monsoon.h"
#include "pwm.h"
#include "app_test.h"
#include "aliot_attr.h"
#include "user_rtc.h"

#define	REGION_MONSOON			"us-west-1"
#define	PKEY_MONSOON			"a3MsurD3c9T"
#define	PSECRET_MONSOON			"HR9rJK2awu8v9LkO"

// length must < 25
#define	PRODUCT_NAME			"ExoMonsoon"
#define	FIRMWARE_VERSION		1

#if	PRODUCT_TYPE == PRODUCT_TYPE_MONSOON
#if	VERSION != FIRMWARE_VERSION
#error "VERSION != FIRMWARE_VERSION"
#endif
#endif

#define	KEY_NUM					1

typedef enum {
	TIMER_DISABLED,
	TIMER_ENABLED,
	TIMER_INVALID
} timer_error_t;

static void user_monsoon_settime(int zone, uint64_t time);

static void user_monsoon_open();
static void user_monsoon_close();

static void user_monsoon_save_config();
static void user_monsoon_para_init();
static void user_monsoon_key_init();
static void user_monsoon_init();
static void user_monsoon_process(void *arg);

static void user_monsoon_pre_smartconfig() ;
static void user_monsoon_post_smartconfig();
static void user_monsoon_pre_apconfig();
static void user_monsoon_post_apconfig();

static key_para_t *pkeys[KEY_NUM];
static key_list_t key_list;
static os_timer_t monsoon_power_timer;

monsoon_config_t monsoon_config;
monsoon_para_t monsoon_para;

static const task_impl_t apc_impl = newTaskImpl(user_monsoon_pre_apconfig, user_monsoon_post_apconfig);
static const task_impl_t sc_impl = newTaskImpl(user_monsoon_pre_smartconfig, user_monsoon_post_smartconfig);

user_device_t user_dev_monsoon = {
	.region = REGION_MONSOON,
	.productKey = PKEY_MONSOON,
	.productSecret = PSECRET_MONSOON,
	.product = PRODUCT_NAME,
	.firmware_version = FIRMWARE_VERSION,

	.key_io_num = KEY_IO_NUM,
	.test_led1_num = LEDR_IO_NUM,
	.test_led2_num = LEDG_IO_NUM,

	.board_init = app_board_monsoon_init,
	.init = user_monsoon_init,
	.process = user_monsoon_process,
	.settime = user_monsoon_settime,

	.attrDeviceInfo = newAttr("DeviceInfo", &user_dev_monsoon.dev_info, NULL, &deviceInfoVtable),
	.attrFirmwareVersion = newIntAttr("FirmwareVersion", &user_dev_monsoon.firmware_version, 1, 65535, &rdIntVtable),
	.attrZone = newIntAttr("Zone", &monsoon_config.super.zone, -720, 720, &defIntVtable),
	.attrDeviceTime = newTextAttr("DeviceTime", user_dev_monsoon.device_time, sizeof(user_dev_monsoon.device_time), &defTextVtable),
	.attrSunrise = newIntAttr("Sunrise", &monsoon_config.super.sunrise, 0, 1439, &defIntVtable),
	.attrSunset = newIntAttr("Sunset", &monsoon_config.super.sunset, 0, 1439, &defIntVtable)
};

static attr_t attrKeyAction = newIntAttr("KeyAction", &monsoon_config.key_action, SPRAY_MIN, SPRAY_MAX, &defIntVtable);
static attr_t attrPower = newIntAttr("Power", &monsoon_para.power, SPRAY_OFF, SPRAY_MAX, &defIntVtable);
static attr_t attrCustomActions = newArrayAttr("CustomActions", &monsoon_config.custom_actions[0], CUSTOM_COUNT, &defIntArrayVtable);
static attr_t attrTimers = newArrayAttr("Timers", &monsoon_config.timers[0], MONSOON_TIMER_MAX, &defIntArrayVtable);

ICACHE_FLASH_ATTR static void user_monsoon_attr_init() {
	aliot_attr_assign(0, &user_dev_monsoon.attrDeviceInfo);
	aliot_attr_assign(1, &user_dev_monsoon.attrFirmwareVersion);
	aliot_attr_assign(2, &user_dev_monsoon.attrZone);
	aliot_attr_assign(3, &user_dev_monsoon.attrDeviceTime);
	aliot_attr_assign(4, &user_dev_monsoon.attrSunrise);
	aliot_attr_assign(5, &user_dev_monsoon.attrSunset);

	aliot_attr_assign(10, &attrKeyAction);
	aliot_attr_assign(11, &attrPower);
	aliot_attr_assign(12, &attrCustomActions);
	aliot_attr_assign(13, &attrTimers);
}

/**
 * @param zone: -720 ~ 720
 * */
ICACHE_FLASH_ATTR static void user_monsoon_settime(int zone, uint64_t time) {
	if (zone < -720 || zone > 720) {
		return;
	}
	user_rtc_set_time(time);
	monsoon_config.super.zone = zone;
	user_dev_monsoon.attrZone.changed = true;

	aliot_attr_post_changed();
	user_monsoon_save_config();
}

ICACHE_FLASH_ATTR static void user_monsoon_ledg_toggle() {
	ledg_toggle();
}

ICACHE_FLASH_ATTR static void user_monsoon_pre_smartconfig() {
	ledr_off();
	aliot_mqtt_disconnect();
	user_indicator_start(SMARTCONFIG_FLASH_PERIOD, 0, user_monsoon_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_monsoon_post_smartconfig() {
	user_indicator_stop();
	ledg_off();
	ledr_on();
}

ICACHE_FLASH_ATTR static void user_monsoon_pre_apconfig() {
	ledr_off();
	aliot_mqtt_disconnect();
	wifi_set_opmode_current(SOFTAP_MODE);
	user_indicator_start(APCONFIG_FLASH_PERIOD, 0, user_monsoon_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_monsoon_post_apconfig() {
	user_indicator_stop();
	ledg_off();
	ledr_on();
}

ICACHE_FLASH_ATTR static void user_monsoon_key_short_press_cb() {
	if (user_smartconfig_instance_status() || user_apconfig_instance_status()) {
		return;
	}
	if (monsoon_para.power > SPRAY_OFF) {
		monsoon_para.power = SPRAY_OFF;
		user_monsoon_close();

		attrPower.changed = true;
	} else {
		if(monsoon_config.key_action <= 0 || monsoon_config.key_action > SPRAY_MAX) {
			monsoon_config.key_action = SPRAY_DEFAULT;
			attrKeyAction.changed = true;
		}
		monsoon_para.power = monsoon_config.key_action;
		user_monsoon_open();
	}

	attrPower.changed = true;
	aliot_attr_post_changed();
}

ICACHE_FLASH_ATTR static void user_monsoon_key_long_press_cb() {
	if (app_test_status()) {					//测试模式
		return;
	}
	if (user_smartconfig_instance_status()) {
		user_smartconfig_instance_stop();
		user_apconfig_instance_start(&apc_impl, APCONFIG_TIMEOUT, user_dev_monsoon.apssid, user_monsoon_settime);
	} else if (user_apconfig_instance_status()) {
		return;
	} else {
		user_smartconfig_instance_start(&sc_impl, SMARTCONFIG_TIEMOUT);
	}
}

ICACHE_FLASH_ATTR static void user_monsoon_key_cont_press_cb() {
	
}

ICACHE_FLASH_ATTR static void user_monsoon_key_release_cb() {
	
}
ICACHE_FLASH_ATTR static void user_monsoon_key_init() {
	pkeys[0] = user_key_init_single(KEY_IO_NUM,
									KEY_IO_FUNC,
									KEY_IO_MUX,
									user_monsoon_key_short_press_cb,
									user_monsoon_key_long_press_cb,
									user_monsoon_key_cont_press_cb,
									user_monsoon_key_release_cb);
	pkeys[0]->long_count = KEY_LONG_PRESS_COUNT;
	key_list.key_num = KEY_NUM;
	key_list.pkeys = pkeys;
	user_key_init_list(&key_list);
}

ICACHE_FLASH_ATTR static timer_error_t user_monsoon_check_timer(monsoon_timer_t *ptmr) {
	if (ptmr == NULL || ptmr->timer > 1439 || ptmr->period < SPRAY_MIN || ptmr->period > SPRAY_MAX) {
		return TIMER_INVALID;
	}
	if (ptmr->enable) {
		return TIMER_ENABLED;
	}
	return TIMER_DISABLED;
}

ICACHE_FLASH_ATTR static void user_monsoon_update_timers() {
	uint8_t i;
	uint8_t cnt = MONSOON_TIMER_MAX;
	for (i = 0; i < MONSOON_TIMER_MAX; i++) {
		monsoon_timer_t *ptmr = &monsoon_config.timers[i];
		if (ptmr->timer > 1439 || ptmr->period < SPRAY_MIN || ptmr->period > SPRAY_MAX) {
			cnt = i;
		}
	}
	for (i = cnt; i < MONSOON_TIMER_MAX; i++) {
		os_memset(&monsoon_config.timers[i], 0, sizeof(monsoon_timer_t));
	}
}

ICACHE_FLASH_ATTR static void user_monsoon_attr_set_cb() {
	if (attrPower.changed = true) {
		if (monsoon_para.power == SPRAY_OFF) {
			user_monsoon_close();
		} else {
			user_monsoon_open();
		}
	}
	// aliot_attr_post_changed();
	user_monsoon_save_config();
}

ICACHE_FLASH_ATTR static void user_monsoon_default_config() {
	uint8_t i;
	os_memset((uint8_t *)&monsoon_config, 0, sizeof(monsoon_config));
	monsoon_config.super.saved_flag = CONFIG_DEFAULT_FLAG;
	monsoon_config.key_action = SPRAY_DEFAULT;
	monsoon_config.super.sunrise = SUNRISE_DEFAULT;
	monsoon_config.super.sunset = SUNSET_DEFAULT;
}

ICACHE_FLASH_ATTR static void user_monsoon_save_config() {
	//测试模式下不改变保存的参数
	if (app_test_status()) {
		return;
	}
	monsoon_config.super.saved_flag = CONFIG_SAVED_FLAG;
	system_param_save_with_protect(PRIV_PARAM_START_SECTOR, &monsoon_config, sizeof(monsoon_config));
}

ICACHE_FLASH_ATTR static void user_monsoon_para_init() {
	uint8_t i;
	system_param_load(PRIV_PARAM_START_SECTOR, 0, &monsoon_config, sizeof(monsoon_config));
	if (monsoon_config.super.saved_flag != CONFIG_SAVED_FLAG) {
		user_monsoon_default_config();
	}
	if (monsoon_config.super.sunrise > TIME_VALUE_MAX) {
		monsoon_config.super.sunrise = SUNRISE_DEFAULT;
	}
	if (monsoon_config.super.sunset > TIME_VALUE_MAX) {
		monsoon_config.super.sunset = SUNSET_DEFAULT;
	}

	if (monsoon_config.key_action < SPRAY_MIN || monsoon_config.key_action > SPRAY_MAX) {
		monsoon_config.key_action = SPRAY_DEFAULT;
	}
	user_monsoon_update_timers();
}

ICACHE_FLASH_ATTR static void user_monsoon_init() {
	ledr_on();
	user_monsoon_para_init();
	user_monsoon_key_init();
	user_monsoon_attr_init();
	aliot_regist_attr_set_cb(user_monsoon_attr_set_cb);
}

ICACHE_FLASH_ATTR static void user_monsoon_process(void *arg) {
	if (user_rtc_is_synchronized() == false) {
		return;
	}
	date_time_t datetime;
	if (user_rtc_get_datetime(&datetime, monsoon_config.super.zone) == false) {
		return;
	}
	bool flag = false;
	bool save = false;
	uint8_t sec = datetime.second;
	if (sec == 0) {
		uint8_t i;
		uint16_t ct = datetime.hour * 60u + datetime.minute;
		uint8_t week = datetime.weekday;
		monsoon_timer_t *ptmr = monsoon_config.timers;
		for (i = 0; i < MONSOON_TIMER_MAX; i++) {
			if (user_monsoon_check_timer(ptmr) == TIMER_ENABLED && ct == ptmr->timer) {
				//once
				if (ptmr->repeat == 0) {
					ptmr->enable = false;
					monsoon_para.power = ptmr->period;
					attrTimers.changed = true;
					flag = true;
					save = true;
				} else if ((ptmr->repeat&(1<<week)) != 0) {
					monsoon_para.power = ptmr->period;
					flag = true;
				}
			}
			ptmr++;
		}
	}
	if (flag) {
		attrPower.changed = true;
		user_monsoon_open();
		aliot_attr_post_changed();
	}
	if (save) {
		user_monsoon_save_config();
	}
}

ICACHE_FLASH_ATTR static void user_monsoon_power_process(void *arg) {
	monsoon_para.power = 0;
	attrPower.changed = true;
	user_monsoon_close();
	aliot_attr_post_changed();
}

ICACHE_FLASH_ATTR static void user_monsoon_open() {
	os_timer_disarm(&monsoon_power_timer);

	gpio_high(CTRL_IO_NUM);
	os_timer_setfn(&monsoon_power_timer, user_monsoon_power_process, NULL);
	os_timer_arm(&monsoon_power_timer, monsoon_para.power*1000, 0);
}

ICACHE_FLASH_ATTR static void user_monsoon_close() {
	os_timer_disarm(&monsoon_power_timer);

	gpio_low(CTRL_IO_NUM);
}