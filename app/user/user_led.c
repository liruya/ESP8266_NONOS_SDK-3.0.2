#include "user_led.h"
#include "app_board_led.h"
#include "pwm.h"
#include "app_test.h"
#include "aliot_attr.h"

//	length must < 25
#define	PRODUCT_NAME			"ExoTerraStrip"

// #define PWM_PERIOD				450					//PWM_PERIOD/DUTY_GAIN=45 450us
// #define DUTY_GAIN				10

#define PWM_PERIOD				4500			//us duty max 100000
#define DUTY_GAIN				100
#define BRIGHT_MIN				0
#define BRIGHT_MAX				1000
#define BRIGHT_DELT				200
#define BRIGHT_MIN2				10
#define BRIGHT_DELT_TOUCH		10
#define BRIGHT_STEP_NORMAL		10

#define TIME_VALUE_MAX			1439

#define LED_STATE_OFF			0
#define LED_STATE_DAY			1
#define LED_STATE_NIGHT			2
#define LED_STATE_WIFI			3

#define	KEY_NUM					1
#define KEY_LONG_TIME_NORMAL	40

static key_para_t *pkeys[KEY_NUM];
static key_list_t key_list;

static uint32_t pwm_io_info[][3] = 	{ 
										{PWM1_IO_MUX, PWM1_IO_FUNC, PWM1_IO_NUM},
										{PWM2_IO_MUX, PWM2_IO_FUNC, PWM2_IO_NUM},
										{PWM3_IO_MUX, PWM3_IO_FUNC, PWM3_IO_NUM},
										{PWM4_IO_MUX, PWM4_IO_FUNC, PWM4_IO_NUM},
										{PWM5_IO_MUX, PWM5_IO_FUNC, PWM5_IO_NUM}
									};

static os_timer_t ramp_timer;

static void user_led_ramp();

static void user_led_off_onShortPress();
static void user_led_day_onShortPress();
static void user_led_night_onShortPress();
static void user_led_wifi_onShortPress();

static void user_led_off_onLongPress();
static void user_led_day_onLongPress();
static void user_led_night_onLongPress();
static void user_led_wifi_onLongPress();

static void user_led_off_onContPress();
static void user_led_day_onContPress();
static void user_led_night_onContPress();
static void user_led_wifi_onContPress();

static void user_led_off_onRelease();
static void user_led_day_onRelease();
static void user_led_night_onRelease();
static void user_led_wifi_onRelease();

static void user_led_onShortPress();
static void user_led_onLongPress();
static void user_led_onContPress();
static void user_led_onRelease();

static void user_led_turnoff_ramp();
static void user_led_turnon_ramp();
static void user_led_turnoff_direct();
static void user_led_turnmax_direct();
static void user_led_update_day_status();
static void user_led_update_night_status();
static void user_led_update_day_bright();
static void user_led_update_night_bright();
static void user_led_indicate_off();
static void user_led_indicate_day();
static void user_led_indicate_night();
static void user_led_indicate_wifi();

static void user_led_init();
static void user_led_process(void *arg);

static void user_led_pre_smartconfig() ;
static void user_led_post_smartconfig();
static void user_led_pre_apconfig();
static void user_led_post_apconfig();

static void user_led_attr_set_cb();

static const key_function_t short_press_func[4] = {	user_led_off_onShortPress,
													user_led_day_onShortPress,
													user_led_night_onShortPress,
													user_led_wifi_onShortPress };
static const key_function_t long_press_func[4] = { 	user_led_off_onLongPress,
													user_led_day_onLongPress,
													user_led_night_onLongPress,
													user_led_wifi_onLongPress };
static const key_function_t cont_press_func[4] = { 	user_led_off_onContPress,
													user_led_day_onContPress,
													user_led_night_onContPress,
													user_led_wifi_onContPress };
static const key_function_t release_func[4] = { user_led_off_onRelease,
												user_led_day_onRelease,
												user_led_night_onRelease,
												user_led_wifi_onRelease };

static const int chn_count = CHANNEL_COUNT;

static led_config_t led_config;

static const task_impl_t apc_impl = newTaskImpl(user_led_pre_apconfig, user_led_post_apconfig);
static const task_impl_t sc_impl = newTaskImpl(user_led_pre_smartconfig, user_led_post_smartconfig);

led_para_t led_para;
user_device_t user_dev_led = {
	.product = PRODUCT_NAME,

	.key_io_num = TOUCH_IO_NUM,
	.test_led1_num = LEDR_IO_NUM,
	.test_led2_num = LEDG_IO_NUM,

	.board_init = app_board_led_init,
	.init = user_led_init,
	.process = user_led_process
};

static attr_t attrDeviceTime = newTextAttr("DeviceTime", user_dev_led.device_time, sizeof(user_dev_led.device_time), &defTextAttrVtable);
static attr_t attrZone = newIntAttr("Zone", &led_config.super.zone, -720, 720, &defIntAttrVtable);
static attr_t attrSunrise = newIntAttr("Sunrise", &led_config.super.sunrise, 0, 1439, &defIntAttrVtable);
static attr_t attrSunset = newIntAttr("Sunset", &led_config.super.sunset, 0, 1439, &defIntAttrVtable);
static attr_t attrChnCount = newIntAttr("ChannelCount", (int *) &chn_count, 0, 6, &defIntAttrVtable);
static attr_t attrChn1Name = newTextAttr("Chn1Name", CHN1_NAME, 32, &defTextAttrVtable);
static attr_t attrChn2Name = newTextAttr("Chn2Name", CHN2_NAME, 32, &defTextAttrVtable);
static attr_t attrChn3Name = newTextAttr("Chn3Name", CHN3_NAME, 32, &defTextAttrVtable);
static attr_t attrChn4Name = newTextAttr("Chn4Name", CHN4_NAME, 32, &defTextAttrVtable);
static attr_t attrChn5Name = newTextAttr("Chn5Name", CHN5_NAME, 32, &defTextAttrVtable);

static attr_t attrMode = newIntAttr("Mode", &led_config.mode, MANUAL, PRO, &defIntAttrVtable);

static attr_t attrPower = newBoolAttr("Power", &led_config.power, &defBoolAttrVtable);
static attr_t attrChn1Bright = newIntAttr("Chn1Bright", &led_config.brights[0], BRIGHT_MIN, BRIGHT_MAX, &defIntAttrVtable);
static attr_t attrChn2Bright = newIntAttr("Chn2Bright", &led_config.brights[1], BRIGHT_MIN, BRIGHT_MAX, &defIntAttrVtable);
static attr_t attrChn3Bright = newIntAttr("Chn3Bright", &led_config.brights[2], BRIGHT_MIN, BRIGHT_MAX, &defIntAttrVtable);
static attr_t attrChn4Bright = newIntAttr("Chn4Bright", &led_config.brights[3], BRIGHT_MIN, BRIGHT_MAX, &defIntAttrVtable);
static attr_t attrChn5Bright = newIntAttr("Chn5Bright", &led_config.brights[4], BRIGHT_MIN, BRIGHT_MAX, &defIntAttrVtable);
static attr_t attrCustom1Brights = newArrayAttr("Custom1Brights", &led_config.custom1Brights[0], CHANNEL_COUNT, &defIntArrayAttrVtable);
static attr_t attrCustom2Brights = newArrayAttr("Custom2Brights", &led_config.custom2Brights[0], CHANNEL_COUNT, &defIntArrayAttrVtable);
static attr_t attrCustom3Brights = newArrayAttr("Custom3Brights", &led_config.custom3Brights[0], CHANNEL_COUNT, &defIntArrayAttrVtable);
static attr_t attrCustom4Brights = newArrayAttr("Custom4Brights", &led_config.custom4Brights[0], CHANNEL_COUNT, &defIntArrayAttrVtable);

static attr_t attrSunriseRamp = newIntAttr("SunriseRamp", &led_config.sunrise_ramp, 0, 240, &defIntAttrVtable);
static attr_t attrSunsetRamp = newIntAttr("SunsetRamp", &led_config.sunset_ramp, 0, 240, &defIntAttrVtable);
static attr_t attrDayBrights = newArrayAttr("DayBrights", &led_config.day_brights, CHANNEL_COUNT, &defIntArrayAttrVtable);
static attr_t attrNightBrights = newArrayAttr("NightBrights", &led_config.night_brights, CHANNEL_COUNT, &defIntArrayAttrVtable);
static attr_t attrTurnoffEnable = newBoolAttr("TurnoffEnable", &led_config.turnoff_enable, &defBoolAttrVtable);
static attr_t attrTurnoffTime = newIntAttr("TurnoffTime", &led_config.turnoff_time, 0, 1439, &defIntAttrVtable);

ICACHE_FLASH_ATTR static void user_led_attr_init() {
	aliot_attr_assign(0, &attrZone);
	aliot_attr_assign(1, &attrSunrise);
	aliot_attr_assign(2, &attrSunset);
	aliot_attr_assign(3, &attrChnCount);
	aliot_attr_assign(4, &attrChn1Name);
	aliot_attr_assign(5, &attrChn2Name);
	aliot_attr_assign(6, &attrChn3Name);
	aliot_attr_assign(7, &attrChn4Name);
	aliot_attr_assign(8, &attrChn5Name);
	aliot_attr_assign(9, &attrMode);
	aliot_attr_assign(10, &attrPower);
	aliot_attr_assign(11, &attrChn1Bright);
	aliot_attr_assign(12, &attrChn2Bright);
	aliot_attr_assign(13, &attrChn3Bright);
	aliot_attr_assign(14, &attrChn4Bright);
	aliot_attr_assign(15, &attrChn5Bright);
	aliot_attr_assign(16, &attrCustom1Brights);
	aliot_attr_assign(17, &attrCustom2Brights);
	aliot_attr_assign(18, &attrCustom3Brights);
	aliot_attr_assign(19, &attrCustom4Brights);
	aliot_attr_assign(20, &attrSunriseRamp);
	aliot_attr_assign(21, &attrSunsetRamp);
	aliot_attr_assign(22, &attrDayBrights);
	aliot_attr_assign(23, &attrNightBrights);
	aliot_attr_assign(24, &attrTurnoffEnable);
	aliot_attr_assign(25, &attrTurnoffTime);
}

ICACHE_FLASH_ATTR static void user_led_setzone(int zone) {
	led_config.super.zone = zone;
	attrZone.changed = true;
}

ICACHE_FLASH_ATTR static void  user_led_ledg_toggle() {
	ledg_toggle();
}

ICACHE_FLASH_ATTR static void  user_led_pre_smartconfig() {
	user_indicator_start(SMARTCONFIG_FLASH_PERIOD, 0, user_led_ledg_toggle);
}

ICACHE_FLASH_ATTR static void  user_led_post_smartconfig() {
	user_indicator_stop();
	ledg_on();
}

ICACHE_FLASH_ATTR static void  user_led_pre_apconfig() {
	wifi_set_opmode_current(SOFTAP_MODE);
	user_indicator_start(APCONFIG_FLASH_PERIOD, 0, user_led_ledg_toggle);
}

ICACHE_FLASH_ATTR static void  user_led_post_apconfig() {
	user_indicator_stop();
	ledg_on();
}

ICACHE_FLASH_ATTR static void  user_led_load_duty(uint32_t value, uint8_t chn) {
	pwm_set_duty(value * DUTY_GAIN, chn);
}

ICACHE_FLASH_ATTR static void  user_led_key_init() {
	pkeys[0] = user_key_init_single(TOUCH_IO_NUM,
							  		TOUCH_IO_FUNC,
									TOUCH_IO_MUX,
									user_led_onShortPress,
									user_led_onLongPress,
									user_led_onContPress,
									user_led_onRelease);
	if (led_config.state == LED_STATE_WIFI) {
		pkeys[0]->long_count = KEY_LONG_PRESS_COUNT;
	} else {
		pkeys[0]->long_count = KEY_LONG_TIME_NORMAL;
	}
	key_list.key_num = KEY_NUM;
	key_list.pkeys = pkeys;
	user_key_init_list(&key_list);
}

ICACHE_FLASH_ATTR static void  user_led_default_config() {
	uint8_t i;
	os_memset(&led_config, 0, sizeof(led_config));

	//super
	led_config.super.sunrise = 420;
	led_config.super.sunset = 1080;

	led_config.last_mode = AUTO;
	led_config.state = LED_STATE_OFF;
	led_config.all_bright = BRIGHT_MAX;
	led_config.blue_bright = BRIGHT_MAX;

	led_config.mode = MANUAL;
	led_config.power = 0;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_config.brights[i] = BRIGHT_MAX;
	}
	
	led_config.sunrise_ramp = 60;
	led_config.sunset_ramp = 60;
	led_config.turnoff_enable = true;
	led_config.turnoff_time = 1320;					//22:00
	for(i = 0; i < CHANNEL_COUNT; i++) {
		led_config.day_brights[i] = 100;
		led_config.night_brights[i] = 0;
	}
	led_config.night_brights[NIGHT_CHANNEL] = 5;
}

ICACHE_FLASH_ATTR static void  user_led_save_config() {
	//测试模式下不改变保存的参数
	if (app_test_status()) {
		return;
	}
	led_config.super.saved_flag = CONFIG_SAVED_FLAG;
	system_param_save_with_protect(PRIV_PARAM_START_SECTOR, &led_config, sizeof(led_config));
}

ICACHE_FLASH_ATTR static void  user_led_para_init() {
	uint8_t i, j;
	system_param_load(PRIV_PARAM_START_SECTOR, 0, &led_config, sizeof(led_config));
	if (led_config.super.saved_flag != CONFIG_SAVED_FLAG) {
		user_led_default_config();
	}

	if (led_config.super.sunrise > TIME_VALUE_MAX) {
		led_config.super.sunrise = 0;
	}
	if (led_config.super.sunset > TIME_VALUE_MAX) {
		led_config.super.sunset = 0;
	}

	if (led_config.mode == AUTO || led_config.mode == PRO) {
		led_config.last_mode = led_config.mode;
	} else {
		led_config.mode = MANUAL;
		if (led_config.last_mode == 0 || led_config.last_mode == 3) {
			led_config.last_mode = AUTO;
		}
	}

	if (led_config.power > 1) {
		led_config.power = 1;
	}
	if (led_config.all_bright > BRIGHT_MAX) {
		led_config.all_bright = BRIGHT_MAX;
	}
	if (led_config.blue_bright > BRIGHT_MAX) {
		led_config.blue_bright = BRIGHT_MAX;
	}
	if (led_config.all_bright < BRIGHT_MIN2) {
		led_config.all_bright = BRIGHT_MIN2;
	}
	if (led_config.blue_bright < BRIGHT_MIN2) {
		led_config.blue_bright = BRIGHT_MIN2;
	}
	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (led_config.brights[i] > BRIGHT_MAX) {
			led_config.brights[i] = BRIGHT_MAX;
		}
	}
	
	if (led_config.sunrise_ramp > 240) {
		led_config.sunrise_ramp = 0;
	}
	if (led_config.sunset_ramp > 240) {
		led_config.sunset_ramp = 0;
	}
	if (led_config.turnoff_time > TIME_VALUE_MAX) {
		led_config.turnoff_time = 0;
	}
	if (led_config.turnoff_enable > 1) {
		led_config.turnoff_enable = 1;
	}
	for(j = 0; j < CHANNEL_COUNT; j++) {
		if(led_config.day_brights[j] > 100) {
			led_config.day_brights[j] = 100;
		}
		if(led_config.night_brights[j] > 100) {
			led_config.night_brights[j] = 100;
		}
	}
}

ICACHE_FLASH_ATTR static void  user_led_init() {
	user_led_para_init();
	user_led_key_init();
	user_led_attr_init();
	aliot_regist_attr_set_cb(user_led_attr_set_cb);
	pwm_init(PWM_PERIOD, led_para.current_bright, CHANNEL_COUNT, pwm_io_info);
	pwm_start();
	switch (led_config.state) {
		case LED_STATE_OFF:
			user_led_indicate_off();
			user_led_turnoff_ramp();
			break;
		case LED_STATE_DAY:
			user_led_indicate_day();
			user_led_update_day_bright();
			break;
		case LED_STATE_NIGHT:
			user_led_indicate_night();
			user_led_update_night_bright();
			break;
		case LED_STATE_WIFI:
			user_led_indicate_wifi();
			if (led_config.mode == MANUAL) {
				if (led_config.power) {
					user_led_turnon_ramp();
				} else {
					user_led_turnoff_ramp();
				}
			}
			break;
		default:
			break;
	}
	os_timer_disarm(&ramp_timer);
	os_timer_setfn(&ramp_timer, user_led_ramp, NULL);
	os_timer_arm(&ramp_timer, 10, 1);
}

ICACHE_FLASH_ATTR static void  user_led_turnoff_ramp() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_para.target_bright[i] = 0;
	}
}

ICACHE_FLASH_ATTR static void  user_led_turnon_ramp() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_para.target_bright[i] = led_config.brights[i];
	}
}

ICACHE_FLASH_ATTR static void  user_led_turnoff_direct() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_para.current_bright[i] = 0;
		led_para.target_bright[i] = 0;
	}
}

ICACHE_FLASH_ATTR static void  user_led_turnmax_direct() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_para.current_bright[i] = BRIGHT_MAX;
		led_para.target_bright[i] = BRIGHT_MAX;
	}
}

ICACHE_FLASH_ATTR static void  user_led_update_day_status() {
	if (led_config.all_bright > BRIGHT_MAX - BRIGHT_DELT) {
		led_para.day_rise = false;
	} else if (led_config.all_bright < BRIGHT_MIN + BRIGHT_DELT) {
		led_para.day_rise = true;
	}
}

ICACHE_FLASH_ATTR static void  user_led_update_night_status() {
	if (led_config.blue_bright > BRIGHT_MAX - BRIGHT_DELT) {
		led_para.night_rise = false;
	} else if (led_config.blue_bright < BRIGHT_MIN + BRIGHT_DELT) {
		led_para.night_rise = true;
	}
}

ICACHE_FLASH_ATTR static void  user_led_update_day_bright() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		led_para.current_bright[i] = led_config.all_bright;
		led_para.target_bright[i] = led_config.all_bright;
		led_config.brights[i] = led_config.all_bright;
	}
}

ICACHE_FLASH_ATTR static void  user_led_update_night_bright() {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (i == NIGHT_CHANNEL) {
			led_para.current_bright[i] = led_config.blue_bright;
			led_para.target_bright[i] = led_config.blue_bright;
			led_config.brights[i] = led_config.blue_bright;
		} else {
			led_para.current_bright[i] = 0;
			led_para.target_bright[i] = 0;
			led_config.brights[i]  = 0;
		}
	}
}

ICACHE_FLASH_ATTR static void  user_led_indicate_off() {
	ledr_on();
	ledg_off();
	ledb_off();
}

ICACHE_FLASH_ATTR static void  user_led_indicate_day() {
	ledr_on();
	ledg_on();
	ledb_on();
}

ICACHE_FLASH_ATTR static void  user_led_indicate_night() {
	ledr_off();
	ledg_off();
	ledb_on();
}

ICACHE_FLASH_ATTR static void  user_led_indicate_wifi() {
	ledr_off();
	ledg_on();
	ledb_off();
}

ICACHE_FLASH_ATTR static void  user_led_off_onShortPress() {
	led_config.state++;
	led_config.power = 1;
	user_led_indicate_day();
	user_led_update_day_bright();
	user_led_save_config();
	
	attrPower.changed = true;
	attrChn1Bright.changed = true;
	attrChn2Bright.changed = true;
	attrChn3Bright.changed = true;
	attrChn4Bright.changed = true;
	attrChn5Bright.changed = true;
	aliot_attr_post_changed();
}

ICACHE_FLASH_ATTR static void  user_led_off_onLongPress() {
}

ICACHE_FLASH_ATTR static void  user_led_off_onContPress() {
}

ICACHE_FLASH_ATTR static void  user_led_off_onRelease() {
}

ICACHE_FLASH_ATTR static void  user_led_day_onShortPress() {
	led_config.state++;
	led_config.power = 1;
	user_led_indicate_night();
	user_led_update_night_bright();

	attrPower.changed = true;
	attrChn1Bright.changed = true;
	attrChn2Bright.changed = true;
	attrChn3Bright.changed = true;
	attrChn4Bright.changed = true;
	attrChn5Bright.changed = true;
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_day_onLongPress() {
	user_led_update_day_status();
}

ICACHE_FLASH_ATTR static void  user_led_day_onContPress() {
	if (led_para.day_rise) {
		if (led_config.all_bright + BRIGHT_DELT_TOUCH <= BRIGHT_MAX) {
			led_config.all_bright += BRIGHT_DELT_TOUCH;
		} else {
			led_config.all_bright = BRIGHT_MAX;
		}
	} else {
		if (led_config.all_bright >= BRIGHT_MIN2 + BRIGHT_DELT_TOUCH) {
			led_config.all_bright -= BRIGHT_DELT_TOUCH;
		} else {
			led_config.all_bright = BRIGHT_MIN2;
		}
	}
	user_led_update_day_bright();
}

ICACHE_FLASH_ATTR static void  user_led_day_onRelease() {
	attrChn1Bright.changed = true;
	attrChn2Bright.changed = true;
	attrChn3Bright.changed = true;
	attrChn4Bright.changed = true;
	attrChn5Bright.changed = true;
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_night_onShortPress() {
	led_config.state++;
	if (led_config.last_mode == PRO) {
		led_config.mode = PRO;
	} else {
		led_config.mode = AUTO;
	}
	pkeys[0]->long_count = KEY_LONG_PRESS_COUNT;
	user_led_indicate_wifi();

	attrMode.changed = true;
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_night_onLongPress() {
	user_led_update_night_status();
}

ICACHE_FLASH_ATTR static void  user_led_night_onContPress() {
	if (led_para.night_rise) {
		if (led_config.blue_bright + BRIGHT_DELT_TOUCH <= BRIGHT_MAX) {
			led_config.blue_bright += BRIGHT_DELT_TOUCH;
		} else {
			led_config.blue_bright = BRIGHT_MAX;
		}
	} else {
		if (led_config.blue_bright >= BRIGHT_MIN2 + BRIGHT_DELT_TOUCH) {
			led_config.blue_bright -= BRIGHT_DELT_TOUCH;
		} else {
			led_config.blue_bright = BRIGHT_MIN2;
		}
	}
	user_led_update_night_bright();
}

ICACHE_FLASH_ATTR static void  user_led_night_onRelease() {
	attrChn1Bright.changed = true;
	attrChn2Bright.changed = true;
	attrChn3Bright.changed = true;
	attrChn4Bright.changed = true;
	attrChn5Bright.changed = true;
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_wifi_onShortPress() {
	if (user_smartconfig_instance_status() || user_apconfig_instance_status()) {
		return;
	}
	led_config.state++;
	led_config.mode = MANUAL;
	led_config.power = 0;
	pkeys[0]->long_count = KEY_LONG_TIME_NORMAL;
	user_led_indicate_off();
	user_led_turnoff_direct();

	attrMode.changed = true;
	attrPower.changed = true;
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_wifi_onLongPress() {
	if (app_test_status()) {					//测试模式
		return;
	}
	if (user_smartconfig_instance_status()) {
		user_smartconfig_instance_stop();
		user_apconfig_instance_start(&apc_impl, APCONFIG_TIMEOUT, user_dev_led.apssid, user_led_setzone);
	} else if (user_apconfig_instance_status()) {
		return;
	} else {
		user_smartconfig_instance_start(&sc_impl, SMARTCONFIG_TIEMOUT);
	}
}

ICACHE_FLASH_ATTR static void  user_led_wifi_onContPress() {
}

ICACHE_FLASH_ATTR static void  user_led_wifi_onRelease() {
}

ICACHE_FLASH_ATTR static void  user_led_onShortPress() {
	short_press_func[led_config.state]();
}

ICACHE_FLASH_ATTR static void  user_led_onLongPress() {
	long_press_func[led_config.state]();
}

ICACHE_FLASH_ATTR static void  user_led_onContPress() {
	cont_press_func[led_config.state]();
}

ICACHE_FLASH_ATTR static void  user_led_onRelease() {
	release_func[led_config.state]();
}

ICACHE_FLASH_ATTR static void  user_led_ramp(void *arg) {
	uint8_t i;
	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (led_para.current_bright[i] + BRIGHT_STEP_NORMAL < led_para.target_bright[i]) {
			led_para.current_bright[i] += BRIGHT_STEP_NORMAL;
		} else if (led_para.current_bright[i] > led_para.target_bright[i] + BRIGHT_STEP_NORMAL) {
			led_para.current_bright[i] -= BRIGHT_STEP_NORMAL;
		} else {
			led_para.current_bright[i] = led_para.target_bright[i];
		}
		user_led_load_duty(led_para.current_bright[i], i);
	}
	pwm_start();
}

ICACHE_FLASH_ATTR static void  user_led_auto_proccess(uint16_t ct, uint8_t sec) {
	if (ct > TIME_VALUE_MAX || sec > 59) {
		return;
	}
	if (led_config.mode != AUTO || led_config.state != LED_STATE_WIFI) {
		return;
	}
	uint8_t i, j, k;
	uint8_t count = 4;
	uint16_t st, et, duration;
	uint32_t dt;
	uint8_t dbrt;
	
	uint16_t sunrise_start = led_config.super.sunrise;
	uint16_t sunrise_ramp = led_config.sunrise_ramp;
	uint16_t sunrise_end = (sunrise_start + sunrise_ramp)%1440;
	uint16_t sunset_end = led_config.super.sunset;
	uint16_t sunset_ramp = led_config.sunset_ramp;
	uint16_t sunset_start = (1440 + sunset_end - sunset_ramp)%1440;

	uint16_t tm[6] = { 	sunrise_start,
						sunrise_end,
						sunset_start,
						sunset_end,
						led_config.turnoff_time,
						led_config.turnoff_time};
	uint8_t brt[6][CHANNEL_COUNT];
	for (i = 0; i < CHANNEL_COUNT; i++) {
		if (led_config.turnoff_enable) {
			brt[0][i] = 0;
		} else {
			brt[0][i] = led_config.night_brights[i];
		}
		brt[1][i] = led_config.day_brights[i];
		brt[2][i] = led_config.day_brights[i];
		brt[3][i] = led_config.night_brights[i];
		brt[4][i] = led_config.night_brights[i];
		brt[5][i] = 0;
	}
	if (led_config.turnoff_enable) {
		count = 6;
	}
	for (i = 0; i < count; i++) {
		j = (i + 1) % count;
		st = tm[i];
		et = tm[j];
		if (et >= st) {
			if (ct >= st && ct < et) {
				duration = et - st;
				dt = (ct - st) * 60u + sec;
			} else {
				continue;
			}
		} else {
			if (ct >= st || ct < et) {
				duration = 1440u - st + et;
				if (ct >= st) {
					dt = (ct - st) * 60u + sec;
				} else {
					dt = (1440u - st + ct) * 60u + sec;
				}
			} else {
				continue;
			}
		}
		for (k = 0; k < CHANNEL_COUNT; k++) {
			if (brt[j][k] >= brt[i][k]) {
				dbrt = brt[j][k] - brt[i][k];
				led_para.target_bright[k] = brt[i][k] * 10u + dbrt * dt / (duration * 6u);
			} else {
				dbrt = brt[i][k] - brt[j][k];
				led_para.target_bright[k] = brt[i][k] * 10u - dbrt * dt / (duration * 6u);
			}
		}
	}
}

ICACHE_FLASH_ATTR static void user_led_attr_set_cb() {
	led_config.state = LED_STATE_WIFI;
	pkeys[0]->long_count = KEY_LONG_PRESS_COUNT;
	user_led_indicate_wifi();
	
	if (led_config.mode == AUTO) {
		led_config.last_mode = AUTO;
	} else if (led_config.mode == PRO) {
		led_config.last_mode = PRO;
	} else {
		led_config.mode = MANUAL;
		if (led_config.power == false) {
			user_led_turnoff_ramp();
		} else {
			user_led_turnon_ramp();
		}
	}
	
	aliot_attr_post_changed();

	user_led_save_config();
}

ICACHE_FLASH_ATTR static void  user_led_process(void *arg) {
	// uint16_t ct = led_para.super.hour * 60u + led_para.super.minute;
	// uint8_t sec = led_para.super.second;
	// if (led_config.mode == AUTO) {
	// 	user_led_auto_proccess(ct, sec);
	// } 
}

