#include "user_socket.h"
#include "app_board_socket.h"
#include "app_test.h"
#include "aliot_attr.h"
#include "user_rtc.h"
#include "user_uart.h"

// length must < 25
#define	PRODUCT_NAME			"ExoTerraSocket"

#define	KEY_NUM					1

#define	SWITCH_SAVED_FLAG		0x12345678

#define	LINKAGE_LOCK_PERIOD		60000		//lock linkage after manual turnon/turnoff
#define	LINKAGE_LOCKON_PERIOD	900000		//lock socket for 15min after turnon

#define FRAME_HEADER			0x68
#define CMD_SET					0x00
#define CMD_GET					0x01

typedef enum _timer_error {
	TIMER_DISABLED, 
	TIMER_ENABLED, 
	TIMER_INVALID
} timer_error_t;

typedef enum _day_night { 
	DAY, 
	NIGHT 
} day_night_t;

static void user_socket_settime(int zone, long time);

static void user_socket_detect_sensor(void *arg);
static void user_socket_decode_sensor(uint8_t *pbuf, uint8_t len);
static void sensor_linkage_process(uint8_t idx, day_night_t day_night);

static void user_socket_save_config();
static void user_socket_para_init();
static void user_socket_key_init();
static void user_socket_init();
static void user_socket_process(void *arg);

static void user_socket_attr_set_cb();

static void user_socket_pre_smartconfig() ;
static void user_socket_post_smartconfig();
static void user_socket_pre_apconfig();
static void user_socket_post_apconfig();

static int 	getTimerString(attr_t *attr, char *text);
static bool parseTimer(attr_t *attr, cJSON *result);
static int 	getSensorString(attr_t *attr, char *text);
static int 	getSensorConfigString(attr_t *attr, char *text);
static bool parseSensorConfig(attr_t *attr, cJSON *result);

static const char *TAG = "Socket";

static key_para_t *pkeys[KEY_NUM];
static key_list_t key_list;
static os_timer_t socket_proc_tmr;
static os_timer_t sensor_detect_tmr;
static os_timer_t linkage_lockon_tmr;
static os_timer_t linkage_lock_tmr;
static volatile bool mLock;				//手动开关后锁定联动标志
static volatile bool mLockon;			//联动打开后锁定标志
static int last_linkage_idx = -1;

static socket_config_t socket_config;

static const task_impl_t apc_impl = newTaskImpl(user_socket_pre_apconfig, user_socket_post_apconfig);
static const task_impl_t sc_impl = newTaskImpl(user_socket_pre_smartconfig, user_socket_post_smartconfig);

socket_para_t socket_para;
user_device_t user_dev_socket = {
	.product = PRODUCT_NAME,

	.key_io_num = KEY_IO_NUM,
	.test_led1_num = LEDR_IO_NUM,
	.test_led2_num = LEDG_IO_NUM,

	.board_init = app_board_socket_init,
	.init = user_socket_init,
	.process = user_socket_process,
	.settime = user_socket_settime,

	.attrZone = newIntAttr("Zone", &socket_config.super.zone, -720, 720, &defIntVtable),
	.attrDeviceTime = newTextAttr("DeviceTime", user_dev_socket.device_time, sizeof(user_dev_socket.device_time), &rdTextVtable),
	.attrSunrise = newIntAttr("Sunrise", &socket_config.super.sunrise, 0, 1439, &defIntVtable),
	.attrSunset = newIntAttr("Sunset", &socket_config.super.sunset, 0, 1439, &defIntVtable)
};

static const int switch_max = SWITCH_COUNT_MAX;

static const attr_vtable_t timerAttrVtable = newAttrVtable(getTimerString, parseTimer);
static const attr_vtable_t sensorAttrVtable = newReadAttrVtable(getSensorString);
static const attr_vtable_t sensorConfigAttrVtable = newAttrVtable(getSensorConfigString, parseSensorConfig);

static attr_t attrSwitchMax = newIntAttr("SwitchCountMax", (int *) &switch_max, 0, 1000000, &rdIntVtable);
static attr_t attrSwitchCount = newIntAttr("SwitchCount", &socket_config.switch_count, 0, 1000000, &defIntVtable);
static attr_t attrMode = newIntAttr("Mode", &socket_config.mode, MODE_TIMER, MODE_SENSOR2, &defIntVtable);
static attr_t attrPower = newBoolAttr("Power", &socket_para.power, &defBoolVtable);
static attr_t attrTimers[SOCKET_TIMER_MAX] = {
	newArrayAttr("Timer1", &socket_config.timers[0], 9, &timerAttrVtable),
	newArrayAttr("Timer2", &socket_config.timers[1], 9, &timerAttrVtable),
	newArrayAttr("Timer3", &socket_config.timers[2], 9, &timerAttrVtable),
	newArrayAttr("Timer4", &socket_config.timers[3], 9, &timerAttrVtable),
	newArrayAttr("Timer5", &socket_config.timers[4], 9, &timerAttrVtable),
	newArrayAttr("Timer6", &socket_config.timers[5], 9, &timerAttrVtable),
	newArrayAttr("Timer7", &socket_config.timers[6], 9, &timerAttrVtable),
	newArrayAttr("Timer8", &socket_config.timers[7], 9, &timerAttrVtable),
	newArrayAttr("Timer9", &socket_config.timers[8], 9, &timerAttrVtable),
	newArrayAttr("Timer10", &socket_config.timers[9], 9, &timerAttrVtable),
	newArrayAttr("Timer11", &socket_config.timers[10], 9, &timerAttrVtable),
	newArrayAttr("Timer12", &socket_config.timers[11], 9, &timerAttrVtable),
	newArrayAttr("Timer13", &socket_config.timers[12], 9, &timerAttrVtable),
	newArrayAttr("Timer14", &socket_config.timers[13], 9, &timerAttrVtable),
	newArrayAttr("Timer15", &socket_config.timers[14], 9, &timerAttrVtable),
	newArrayAttr("Timer16", &socket_config.timers[15], 9, &timerAttrVtable),
	newArrayAttr("Timer17", &socket_config.timers[16], 9, &timerAttrVtable),
	newArrayAttr("Timer18", &socket_config.timers[17], 9, &timerAttrVtable),
	newArrayAttr("Timer19", &socket_config.timers[18], 9, &timerAttrVtable),
	newArrayAttr("Timer20", &socket_config.timers[19], 9, &timerAttrVtable),
	newArrayAttr("Timer21", &socket_config.timers[20], 9, &timerAttrVtable),
	newArrayAttr("Timer22", &socket_config.timers[21], 9, &timerAttrVtable),
	newArrayAttr("Timer23", &socket_config.timers[22], 9, &timerAttrVtable),
	newArrayAttr("Timer24", &socket_config.timers[23], 9, &timerAttrVtable)
};
// static attr_t attrTimer1 = newArrayAttr("Timer1", &socket_config.timers[0], 9, &timerAttrVtable);
// static attr_t attrTimer2 = newArrayAttr("Timer2", &socket_config.timers[1], 9, &timerAttrVtable);
// static attr_t attrTimer3 = newArrayAttr("Timer3", &socket_config.timers[2], 9, &timerAttrVtable);
// static attr_t attrTimer4 = newArrayAttr("Timer4", &socket_config.timers[3], 9, &timerAttrVtable);
// static attr_t attrTimer5 = newArrayAttr("Timer5", &socket_config.timers[4], 9, &timerAttrVtable);
// static attr_t attrTimer6 = newArrayAttr("Timer6", &socket_config.timers[5], 9, &timerAttrVtable);
// static attr_t attrTimer7 = newArrayAttr("Timer7", &socket_config.timers[6], 9, &timerAttrVtable);
// static attr_t attrTimer8 = newArrayAttr("Timer8", &socket_config.timers[7], 9, &timerAttrVtable);
// static attr_t attrTimer9 = newArrayAttr("Timer9", &socket_config.timers[8], 9, &timerAttrVtable);
// static attr_t attrTimer10 = newArrayAttr("Timer10", &socket_config.timers[9], 9, &timerAttrVtable);
// static attr_t attrTimer11 = newArrayAttr("Timer11", &socket_config.timers[10], 9, &timerAttrVtable);
// static attr_t attrTimer12 = newArrayAttr("Timer12", &socket_config.timers[11], 9, &timerAttrVtable);
// static attr_t attrTimer13 = newArrayAttr("Timer13", &socket_config.timers[12], 9, &timerAttrVtable);
// static attr_t attrTimer14 = newArrayAttr("Timer14", &socket_config.timers[13], 9, &timerAttrVtable);
// static attr_t attrTimer15 = newArrayAttr("Timer15", &socket_config.timers[14], 9, &timerAttrVtable);
// static attr_t attrTimer16 = newArrayAttr("Timer16", &socket_config.timers[15], 9, &timerAttrVtable);
// static attr_t attrTimer17 = newArrayAttr("Timer17", &socket_config.timers[16], 9, &timerAttrVtable);
// static attr_t attrTimer18 = newArrayAttr("Timer18", &socket_config.timers[17], 9, &timerAttrVtable);
// static attr_t attrTimer19 = newArrayAttr("Timer19", &socket_config.timers[18], 9, &timerAttrVtable);
// static attr_t attrTimer20 = newArrayAttr("Timer20", &socket_config.timers[19], 9, &timerAttrVtable);
// static attr_t attrTimer21 = newArrayAttr("Timer21", &socket_config.timers[20], 9, &timerAttrVtable);
// static attr_t attrTimer22 = newArrayAttr("Timer22", &socket_config.timers[21], 9, &timerAttrVtable);
// static attr_t attrTimer23 = newArrayAttr("Timer23", &socket_config.timers[22], 9, &timerAttrVtable);
// static attr_t attrTimer24 = newArrayAttr("Timer24", &socket_config.timers[23], 9, &timerAttrVtable);
// static attr_t attrTimers = newArrayAttr("Timers", &socket_config.timers[0], SOCKET_TIMER_MAX, &timerAttrVtable);
static attr_t attrSensorAvailable = newBoolAttr("SensorAvailable", &socket_para.sensor_available, &rdBoolVtable);
static attr_t attrSensor = newArrayAttr("Sensor", &socket_para.sensor[0], SENSOR_COUNT_MAX, &sensorAttrVtable);
static attr_t attrSensorConfig = newArrayAttr("SensorConfig", &socket_config.sensor_config[0], SENSOR_COUNT_MAX, &sensorConfigAttrVtable);

ICACHE_FLASH_ATTR static void user_socket_attr_init() {
	uint8_t i;

	aliot_attr_assign(0, &user_dev_socket.attrZone);
	aliot_attr_assign(1, &user_dev_socket.attrDeviceTime);
	aliot_attr_assign(2, &user_dev_socket.attrSunrise);
	aliot_attr_assign(3, &user_dev_socket.attrSunset);

	aliot_attr_assign(10, &attrSwitchMax);
	aliot_attr_assign(11, &attrSwitchCount);
	aliot_attr_assign(12, &attrMode);
	aliot_attr_assign(13, &attrPower);
	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
		aliot_attr_assign(14+i, &attrTimers[i]);
	}
	// aliot_attr_assign(14, &attrTimer1);
	// aliot_attr_assign(15, &attrTimer2);
	// aliot_attr_assign(16, &attrTimer3);
	// aliot_attr_assign(17, &attrTimer4);
	// aliot_attr_assign(18, &attrTimer5);
	// aliot_attr_assign(19, &attrTimer6);
	// aliot_attr_assign(20, &attrTimer7);
	// aliot_attr_assign(21, &attrTimer8);
	// aliot_attr_assign(22, &attrTimer9);
	// aliot_attr_assign(23, &attrTimer10);
	// aliot_attr_assign(24, &attrTimer11);
	// aliot_attr_assign(25, &attrTimer12);
	// aliot_attr_assign(26, &attrTimer13);
	// aliot_attr_assign(27, &attrTimer14);
	// aliot_attr_assign(28, &attrTimer15);
	// aliot_attr_assign(29, &attrTimer16);
	// aliot_attr_assign(30, &attrTimer17);
	// aliot_attr_assign(31, &attrTimer18);
	// aliot_attr_assign(32, &attrTimer19);
	// aliot_attr_assign(33, &attrTimer20);
	// aliot_attr_assign(34, &attrTimer21);
	// aliot_attr_assign(35, &attrTimer22);
	// aliot_attr_assign(36, &attrTimer23);
	// aliot_attr_assign(37, &attrTimer24);
	aliot_attr_assign(38, &attrSensorAvailable);
	aliot_attr_assign(39, &attrSensor);
	aliot_attr_assign(40, &attrSensorConfig);
}

/**************************************************************************************************
 * 
 * SmartConfig & APConfig
 * 
 * ***********************************************************************************************/

/**
 * @param zone: -720 ~ 720
 * */
ICACHE_FLASH_ATTR static void user_socket_settime(int zone, long time) {
	if (zone < -720 || zone > 720) {
		return;
	}
	user_rtc_set_time(time);
	socket_config.super.zone = zone;
	user_dev_socket.attrZone.changed = true;

	user_socket_save_config();
}

ICACHE_FLASH_ATTR static void user_socket_ledg_toggle() {
	ledg_toggle();
}

ICACHE_FLASH_ATTR static void user_socket_pre_smartconfig() {
	ledr_off();
	ledb_off();
	aliot_mqtt_disconnect();
	user_indicator_start(SMARTCONFIG_FLASH_PERIOD, 0, user_socket_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_socket_post_smartconfig() {
	user_indicator_stop();
	ledg_off();
	if (relay_status()) {
		ledb_on();
	} else {
		ledr_on();
	}
}

ICACHE_FLASH_ATTR static void user_socket_pre_apconfig() {
	ledr_off();
	ledb_off();
	aliot_mqtt_disconnect();
	wifi_set_opmode_current(SOFTAP_MODE);
	user_indicator_start(APCONFIG_FLASH_PERIOD, 0, user_socket_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_socket_post_apconfig() {
	user_indicator_stop();
	ledg_off();
	if (relay_status()) {
		ledb_on();
	} else {
		ledr_on();
	}
}

/*************************************************************************************************/

ICACHE_FLASH_ATTR static void user_socket_linkage_lockon_action(void *arg) {
	mLockon = false;
	LOGD(TAG, "unlockon");
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lockon_start() {
	os_timer_disarm(&linkage_lockon_tmr);
	os_timer_setfn(&linkage_lockon_tmr, user_socket_linkage_lockon_action, NULL);
	mLockon = true;
	os_timer_arm(&linkage_lockon_tmr, LINKAGE_LOCKON_PERIOD, 0);
	LOGD(TAG, "lockon");
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lock_action(void *arg) {
	mLock = false;
	LOGD(TAG, "unlock");
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lock_start() {
	os_timer_disarm(&linkage_lock_tmr);
	os_timer_setfn(&linkage_lock_tmr, user_socket_linkage_lock_action, NULL);
	mLock = true;
	os_timer_arm(&linkage_lock_tmr, LINKAGE_LOCK_PERIOD, 0);
	LOGD(TAG, "lock");
}

/**
 * off->on return true
 * on->on return false
 * */
ICACHE_FLASH_ATTR static bool user_socket_turnon() {
	LOGD(TAG, "power: %d relay: %d", socket_para.power, relay_status());
	if (relay_status() == false) {
		relay_on();
		ledr_off();
		ledb_on();
		socket_config.switch_flag = SWITCH_SAVED_FLAG;
		socket_config.switch_count++;
		attrSwitchCount.changed = true;
		return true;
	}
	return false;
}

/**
 * on->off return true
 * off->off return false
 * */
ICACHE_FLASH_ATTR static bool user_socket_turnoff() {
	LOGD(TAG, "power: %d relay: %d", socket_para.power, relay_status());
	if (relay_status()) {
		relay_off();
		ledr_on();
		ledb_off();
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR static bool user_socket_turnon_manual() {
	if (user_socket_turnon()) {		//手动开,锁定联动动作
		user_socket_linkage_lock_start();
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR static bool user_socket_turnon_linkage() {
	if (user_socket_turnon()) {		//联动开,联动锁定打开周期
		user_socket_linkage_lockon_start();
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR static bool user_socket_turnoff_manual() {
	if (user_socket_turnoff()) {	//手动关,锁定联动动作
		user_socket_linkage_lock_start();
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR static bool user_socket_turnoff_linkage() {
	return user_socket_turnoff();
}

/* ***********************************************************************************************
 * 
 * key functions
 * 
 * ***********************************************************************************************/

ICACHE_FLASH_ATTR static void user_socket_key_short_press_cb() {	
	if (user_smartconfig_instance_status() || user_apconfig_instance_status()) {
		return;
	}
	
	socket_para.power = !socket_para.power;
	if (socket_para.power) {
		if (user_socket_turnon_manual()) {
			user_socket_save_config();
		}
	} else {
		user_socket_turnoff_manual();
	}

	attrPower.changed = true;
	aliot_attr_post_changed();
}

ICACHE_FLASH_ATTR static void user_socket_key_long_press_cb() {
	if (app_test_status()) {				//测试模式
		return;	
	}
	if (user_smartconfig_instance_status()) {
		user_smartconfig_instance_stop();
		user_apconfig_instance_start(&apc_impl, APCONFIG_TIMEOUT, user_dev_socket.apssid, user_socket_settime);
	} else if (user_apconfig_instance_status()) {
		return;
	} else {
		user_smartconfig_instance_start(&sc_impl, SMARTCONFIG_TIEMOUT);
	}
}

ICACHE_FLASH_ATTR static void user_socket_key_cont_press_cb() {
}

ICACHE_FLASH_ATTR static void user_socket_key_release_cb() {
}

ICACHE_FLASH_ATTR static void user_socket_key_init() {
	pkeys[0] = user_key_init_single(KEY_IO_NUM,
								  	KEY_IO_FUNC,
									KEY_IO_MUX,
									user_socket_key_short_press_cb,
									user_socket_key_long_press_cb,
									user_socket_key_cont_press_cb,
									user_socket_key_release_cb);
	pkeys[0]->long_count = KEY_LONG_PRESS_COUNT;
	key_list.key_num = KEY_NUM;
	key_list.pkeys = pkeys;
	user_key_init_list(&key_list);
}

/*************************************************************************************************/


ICACHE_FLASH_ATTR static timer_error_t user_socket_check_timer(socket_timer_t *p_timer) {
	if (p_timer == NULL) {
		return TIMER_INVALID;
	}
	if (p_timer->action > ACTION_TURNON_DURATION 
		|| p_timer->hour > 23 || p_timer->minute > 59 || p_timer->second > 59
		|| p_timer->end_hour > 23 || p_timer->end_minute > 59 || p_timer->end_second > 59) {
		return TIMER_INVALID;
	}
	if (p_timer->enable) {
		return TIMER_ENABLED;
	}
	return TIMER_DISABLED;
}

ICACHE_FLASH_ATTR static void user_socket_reset_timer(socket_timer_t *p_timer) {
	if (p_timer == NULL) {
		return;
	}
	os_memset(p_timer, 0xFF, sizeof(socket_timer_t));
}

ICACHE_FLASH_ATTR static void user_socket_update_timers() {
	uint8_t i;
	uint8_t cnt = SOCKET_TIMER_MAX;
	for(i = 0; i < SOCKET_TIMER_MAX; i++) {
		if(user_socket_check_timer(&socket_config.timers[i]) == TIMER_INVALID) {
			cnt = i;
			break;
		}
	}
	for(i = cnt; i < SOCKET_TIMER_MAX; i++) {
		os_memset(&socket_config.timers[i], 0xFF, sizeof(socket_timer_t));
	}
}

ICACHE_FLASH_ATTR static void user_socket_default_config() {
	uint8_t i;
	system_param_load(PRIV_PARAM_START_SECTOR, 0, &socket_config, sizeof(socket_config));
	uint32_t sw_flag = socket_config.switch_flag;
	uint32_t sw_count = socket_config.switch_count;
	os_memset((uint8_t *)&socket_config, 0, sizeof(socket_config));
	socket_config.super.saved_flag = CONFIG_DEFAULT_FLAG;
	if(sw_flag == SWITCH_SAVED_FLAG && sw_count <= SWITCH_COUNT_MAX) {
		socket_config.switch_count = sw_count;
	} else {
		socket_config.switch_count = 0;
	}
	socket_config.switch_flag = SWITCH_SAVED_FLAG;
	socket_config.mode = MODE_TIMER;
	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
		os_memset(&socket_config.timers[i], 0xFF, sizeof(socket_timer_t));
	}

	socket_config.super.sunrise = 420;
	socket_config.super.sunset = 1080;
}

ICACHE_FLASH_ATTR static void user_socket_save_config() {
	//测试模式下不改变保存的参数
	if (app_test_status()) {
		return;
	}
	socket_config.super.saved_flag = CONFIG_SAVED_FLAG;
	system_param_save_with_protect(PRIV_PARAM_START_SECTOR, &socket_config, sizeof(socket_config));
}

ICACHE_FLASH_ATTR static void user_socket_para_init() {
	uint8_t i;
	system_param_load(PRIV_PARAM_START_SECTOR, 0, &socket_config, sizeof(socket_config));
	if (socket_config.super.saved_flag != CONFIG_SAVED_FLAG) {
		user_socket_default_config();
	}
	if(socket_config.switch_flag != SWITCH_SAVED_FLAG || socket_config.switch_count > SWITCH_COUNT_MAX) {
		socket_config.switch_flag = SWITCH_SAVED_FLAG;
		socket_config.switch_count = 0;
	}
	user_socket_update_timers();
	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
		socket_config.timers[i].repeat &= 0x7F;
	}
}

ICACHE_FLASH_ATTR static void user_socket_init() {
	ledr_on();
	uart0_init(BAUDRATE_9600, 16);
	uart0_set_rx_cb(user_socket_decode_sensor);
	uart_enable_isr();

	user_socket_para_init();
	user_socket_key_init();
	user_socket_attr_init();
	aliot_regist_attr_set_cb(user_socket_attr_set_cb);
	os_timer_disarm(&sensor_detect_tmr);
	os_timer_setfn(&sensor_detect_tmr, user_socket_detect_sensor, NULL);
	os_timer_arm(&sensor_detect_tmr, 32, 1);
}

ICACHE_FLASH_ATTR static void user_socket_process(void *arg) {
	if (user_smartconfig_instance_status() || user_apconfig_instance_status()) {
		return;
	}
	if (user_rtc_is_synchronized() == false) {
		return;
	}
	date_time_t datetime;
	bool result = user_rtc_get_datetime(&datetime, socket_config.super.zone);
	if (result == false) {
		return;
	}

	if (socket_config.mode == MODE_SENSOR1 || socket_config.mode == MODE_SENSOR2) {
		if (socket_para.sensor_available == false) {
			return;
		}
		day_night_t day_night = NIGHT;
		uint16_t sunrise = socket_config.super.sunrise;
		uint16_t sunset = socket_config.super.sunset;
		uint16_t ct = datetime.hour*60 + datetime.minute;
		if (sunrise < sunset && (ct >= sunrise && ct < sunset)) {
			day_night = DAY;
		} else if (sunrise > sunset && (ct >= sunrise || ct < sunset)) {
			day_night = DAY;
		}
		sensor_linkage_process(socket_config.mode-1, day_night);
		return;
	}
	last_linkage_idx = -1;

	uint8_t i;
	socket_timer_t *p;
	bool flag = false;
	bool save = false;
	bool action = false;
	uint8_t week = datetime.weekday;
	uint8_t hour = datetime.hour;
	uint8_t minute = datetime.minute;
	uint8_t second = datetime.second;
	timer_error_t error;
	user_socket_update_timers();
	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
		p = &socket_config.timers[i];
		error = user_socket_check_timer(p);
		if (error == TIMER_INVALID) {
			break;
		}
		if (error == TIMER_ENABLED) {
			if (p->hour == hour && p->minute == minute && p->second == second) {
				if (p->repeat == 0) {
					action = (p->action > ACTION_TURNOFF ? true : false);
					flag = true;
					if (p->action == ACTION_TURNON_DURATION) {
						p->repeat = 0x80; 
					} else {
						p->enable = 0;
						attrTimers[i].changed = true;
						save = true;
					}
				} else if ((p->repeat&(1<<week)) != 0) {
					action = (p->action > ACTION_TURNOFF ? true : false);
					flag = true;
					if (p->action == ACTION_TURNON_DURATION) {
						p->repeat |= 0x80; 
					}
				}
			}
			if (p->action == ACTION_TURNON_DURATION && p->end_hour == hour && p->end_minute == minute && p->end_second == second) { 
				if (p->repeat == 0x80) {
					p->repeat = 0;
					p->enable = false;
					action = false;
					flag = true;
					attrTimers[i].changed = true;
					save = true;
				} else if (p->repeat > 0x80) {
					p->repeat &= 0x7F;
					action = false;
					flag = true;
				}
			}
		}
	}
	if (flag) {
		socket_para.power = action;
		if(action) {
			if (user_socket_turnon_manual()) {
				save = true;
			}
		} else {
			user_socket_turnoff_manual();
		}
		if(save) {
			user_socket_save_config();
		}
		attrPower.changed = true;
		aliot_attr_post_changed();
	}
}

ICACHE_FLASH_ATTR static bool sensor_config_check() {
	uint8_t i;
	if (socket_para.sensor_available) {
		for (i = 0; i < SENSOR_COUNT_MAX; i++) {
			sensor_t *psensor = &socket_para.sensor[i];
			sensor_config_t *pcfg = &socket_config.sensor_config[i];
			if (pcfg->type != psensor->type) {
				return false;
			}
			if (pcfg->ntfyEnable > 1) {
				return false;
			}
			if (pcfg->ntfyThrdLower >= pcfg->ntfyThrdUpper) {
				return false;
			}
			if (pcfg->ntfyThrdLower < psensor->thrdLower 
				|| pcfg->ntfyThrdUpper > psensor->thrdUpper) {
				return false;
			}
			if (pcfg->dayConst < psensor->thrdLower 
				|| pcfg->dayConst > psensor->thrdUpper) {
				return false;		
			}
			if (pcfg->nightConst < psensor->thrdLower 
				|| pcfg->nightConst > psensor->thrdUpper) {
				return false;
			}
		}
		return true;
	}
	return false;
}

/**
 * @param idx: 传感器索引
 * @param day_night: DAY or NIGHT
 * */
ICACHE_FLASH_ATTR static void sensor_linkage_process(uint8_t idx, day_night_t day_night) {
	if (idx >= SENSOR_COUNT_MAX) {
		return;
	}
	static uint8_t overCount[SENSOR_COUNT_MAX] = {0};
	static uint8_t overRecoverCount[SENSOR_COUNT_MAX] = {0};
	static uint8_t lossCount[SENSOR_COUNT_MAX] = {0};
	static uint8_t lossRecoverCount[SENSOR_COUNT_MAX] = {0};
	static bool overFlag[SENSOR_COUNT_MAX] = {false};
	static bool lossFlag[SENSOR_COUNT_MAX] = {false};
	// static uint8_t last_idx = 0;

	if (idx != last_linkage_idx) {
		overCount[idx] = 0;
		overRecoverCount[idx] = 0;
		lossCount[idx] = 0;
		lossRecoverCount[idx] = 0;
		overFlag[idx] = false;
		lossFlag[idx] = false;

		last_linkage_idx = idx;
	}

	sensor_t *psensor = &socket_para.sensor[idx];
	sensor_config_t *pcfg = &socket_config.sensor_config[idx];
	uint8_t range = psensor->range;

	int32_t target = (day_night == DAY ? pcfg->dayConst : pcfg->nightConst);
	int32_t max = (target+range)*10;
	int32_t min = (target-range)*10;
	int32_t temp = psensor->value;

	/* check sensor over flag */
	if (overFlag[idx] == false) {
		if (temp > max) {
			overCount[idx]++;
			if (overCount[idx] > 60) {
				overFlag[idx] = true;
				overCount[idx] = 0;

				LOGD(TAG, "sensor%d overflag: 1", (idx+1));
			}
		} else {
			overCount[idx] = 0;
		}
	} else {
		if (temp < max) {
			overCount[idx]++;
			if (overCount[idx] > 60) {
				overFlag[idx] = false;
				overCount[idx] = 0;

				LOGD(TAG, "sensor%d overflag: 0", (idx+1));
			}
		} else {
			overCount[idx] = 0;
		}
	}

	/* check sensor loss flag */
	if (lossFlag[idx] == false) {
		if (temp < min) {
			lossCount[idx]++;
			if (lossCount[idx] > 60) {
				lossFlag[idx] = true;
				lossCount[idx] = 0;

				LOGD(TAG, "sensor%d lossflag: 1", (idx+1));
			}
		} else {
			lossCount[idx] = 0;
		}
	} else {
		if (temp > min) {
			lossCount[idx]++;
			if (lossCount[idx] > 60) {
				lossFlag[idx] = false;
				lossCount[idx] = 0;

				LOGD(TAG, "sensor%d lossflag: 0", (idx+1));
			}
		} else {
			lossCount[idx] = 0;
		}
	}

	// LOGD(TAG, "val: %d min: %d max: %d over1: %d loss1: %d over2: %d loss2: %d", 
	// 	temp, min, max, overFlag[0], lossFlag[0], overFlag[1], lossFlag[1]);

	if (mLock) {
		return;
	}

	if (overFlag[idx]) {
		if (user_socket_turnoff_linkage()) {
			socket_para.power = false;

			attrPower.changed = true;
			aliot_attr_post_changed();
		}
		return;
	}
	if (lossFlag[idx] && !mLockon) {
		if (user_socket_turnon_linkage()) {
			socket_para.power = true;

			attrPower.changed = true;
			aliot_attr_post_changed();
			user_socket_save_config();
		}
	}
}

ICACHE_FLASH_ATTR static void user_socket_attr_set_cb() {
	if (socket_para.power) {
		user_socket_turnon_manual();
	} else {
		user_socket_turnoff_manual();
	}
	aliot_attr_post_changed();

	user_socket_save_config();
}

/* ************************************************************************************************
 * 
 * socket <-> sensor
 * 
 * ***********************************************************************************************/

ICACHE_FLASH_ATTR static void user_socket_detect_sensor(void *arg) {
	static bool m_sensor_detected;
	static uint8_t trg;
	static uint8_t cont;
	uint8_t rd = GPIO_INPUT_GET(DETECT_IO_NUM) ^ 0x01 ;
	trg = rd & (rd ^ cont);
	cont = rd;
	bool detect = trg ^ cont;
	if (m_sensor_detected == true && detect == false) {
		m_sensor_detected = false;
		socket_para.sensor_available = false;
		socket_config.mode = MODE_TIMER;
		attrSensorAvailable.changed = true;
		attrMode.changed = true;

		aliot_attr_post_changed();
		user_socket_save_config();

		LOGD(TAG, "sensor removed...");
	} else if (m_sensor_detected == false && detect == true) {
		m_sensor_detected = true;

		LOGD(TAG, "sensor detected...");
	}
}

ICACHE_FLASH_ATTR static int get_sensor_range(int type) {
	int range = 1;
	switch (type) {
		case SENSOR_TEMPERATURE:
			range = TEMPERATURE_RANGE;
			break;
		case SENSOR_HUMIDITY:
			range = HUMIDITY_RANGE;
			break;
		default:
			break;
	}
	return range;
}

ICACHE_FLASH_ATTR static int get_sensor_thrdlower(int type) {
	int thrdlower = 0;
	switch (type) {
		case SENSOR_TEMPERATURE:
			thrdlower = TEMPERATURE_THRDLOWER;
			break;
		case SENSOR_HUMIDITY:
			thrdlower = HUMIDITY_THRDLOWER;
			break;
		default:
			break;
	}
	return thrdlower;
}

ICACHE_FLASH_ATTR static int get_sensor_thrdupper(int type) {
	int thrdupper = 0;
	switch (type) {
		case SENSOR_TEMPERATURE:
			thrdupper = TEMPERATURE_THRDUPPER;
			break;
		case SENSOR_HUMIDITY:
			thrdupper = HUMIDITY_THRDUPPER;
			break;
		default:
			break;
	}
	return thrdupper;
}

ICACHE_FLASH_ATTR static bool check_sensor(uint8_t idx) {
	if (socket_para.sensor_available == false || idx >= SENSOR_COUNT_MAX) {
		return false;
	}
	if (socket_para.sensor[idx].type == 0) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR static bool check_sensor_config(uint8_t idx) {
	if (socket_para.sensor_available == false || idx >= SENSOR_COUNT_MAX) {
		return false;
	}
	bool changed = false;
	sensor_t *psensor = &socket_para.sensor[idx];
	sensor_config_t *pcfg = &socket_config.sensor_config[idx];
	if (psensor->type == 0) {
		return false;
	}
	if (pcfg->type != psensor->type || pcfg->type == 0) {
		pcfg->type = psensor->type;
		changed = true;
	}
	if (pcfg->dayConst < psensor->thrdLower || pcfg->dayConst > psensor->thrdUpper) {
		pcfg->dayConst = (psensor->thrdLower+psensor->thrdUpper)/2;
		changed = true;
	}
	if (pcfg->nightConst < psensor->thrdLower || pcfg->nightConst > psensor->thrdUpper) {
		pcfg->nightConst = (psensor->thrdLower+psensor->thrdUpper)/2;
		changed = true;
	}
	if (pcfg->ntfyThrdLower < psensor->thrdLower || pcfg->ntfyThrdLower > psensor->thrdUpper) {
		pcfg->ntfyThrdLower = psensor->thrdLower;
		changed = true;
	}
	if (pcfg->ntfyThrdUpper < psensor->thrdLower || pcfg->ntfyThrdUpper > psensor->thrdUpper) {
		pcfg->ntfyThrdUpper = psensor->thrdUpper;
		changed = true;
	}
	if (pcfg->ntfyThrdLower > pcfg->ntfyThrdUpper) {
		pcfg->ntfyThrdLower = psensor->thrdLower;
		pcfg->ntfyThrdUpper = psensor->thrdUpper;
		changed = true;
	}
	if (pcfg->ntfyEnable > 1) {
		pcfg->ntfyEnable = false;
		changed = true;
	}
	if (changed) {
		attrSensorConfig.changed = true;
	}
	return true;
}

ICACHE_FLASH_ATTR uint32_t myabs(int32_t val) {
	if (val >= 0) {
		return val;
	}
	return (0-val);
}

ICACHE_FLASH_ATTR void update_sensor(uint8_t idx, int type, int value) {
	if (idx >= SENSOR_COUNT_MAX) {
		return;
	}
	socket_para.sensor[idx].type = type;
	socket_para.sensor[idx].value = value;
	socket_para.sensor[idx].thrdLower = get_sensor_thrdlower(type);
	socket_para.sensor[idx].thrdUpper = get_sensor_thrdupper(type);
	socket_para.sensor[idx].range = get_sensor_range(type);
	attrSensor.changed = true;
	check_sensor_config(idx);
}

#define	POST_INTERVAL	900			//	传感器数据上报间隔 15min
#define	POST_DELTA		3			//	和上次上报数据差值大于3上报
/**
 * @param pbuf: receive data buf for sensor
 * @param len: data buf len
 * get:	FRM_HDR CMD_GET XOR
 * rsp: FRM_HDR CMD_GET type1 value1(4) {type2 value2(4)} XOR
 * */
ICACHE_FLASH_ATTR static void user_socket_decode_sensor(uint8_t *pbuf, uint8_t len) {
	static uint64_t lastTime;
	static int32_t lastValue1;
	static int32_t lastValue2;
	if(pbuf == NULL) {
		return;
	}
	if (len != 8 && len != 13) {
		return;
	}
	if(pbuf[0] != FRAME_HEADER || pbuf[1] != CMD_GET) {
		return;
	}
	uint8_t i;
	uint8_t xor = 0;
	for (i = 0; i < len; i++) {
		xor ^= pbuf[i];
	}
	if(xor != 0) {
		return;
	}
	uint64_t time = user_rtc_get_time()/1000;
	bool changed = false;
	uint8_t type1 = pbuf[2];
	int32_t value1 = (pbuf[6]<<24)|(pbuf[5]<<16)|(pbuf[4]<<8)|pbuf[3];
	uint8_t type2 = 0;
	int32_t value2 = 0;
	if (len == 13) {
		type2 = pbuf[7];
		value2 = (pbuf[11]<<24)|(pbuf[10]<<16)|(pbuf[9]<<8)|pbuf[8];
	}
	if (socket_para.sensor_available == false) {
		socket_para.sensor_available = true;
		attrSensorAvailable.changed = true;

		changed = true;
	} else {
		uint32_t delta1 = myabs(value1-lastValue1);
		uint32_t delta2 = myabs(value2-lastValue2);
		if (time - lastTime >= POST_INTERVAL
			|| delta1 >= POST_DELTA
			|| delta2 >= POST_DELTA) {
			changed = true;
		}
	}
	if (changed) {
		lastValue1 = value1;
		lastValue2 = value2;
		lastTime = time;
		update_sensor(0, type1, value1);
		update_sensor(1, type2, value2);
		aliot_attr_post_changed();
	}
	// if (socket_para.sensor[0].type != type1 || socket_para.sensor[0].value != value1) {
	// 	socket_para.sensor[0].type = type1;
	// 	socket_para.sensor[0].value = value1;
	// 	socket_para.sensor[0].thrdLower = get_sensor_thrdlower(type1);
	// 	socket_para.sensor[0].thrdUpper = get_sensor_thrdupper(type1);
	// 	socket_para.sensor[0].range = get_sensor_range(type1);
	// 	attrSensor.changed = true;
	// 	check_sensor_config(0);
	// }
	// if (socket_para.sensor[1].type != type2 || socket_para.sensor[1].value != value2) {
	// 	socket_para.sensor[1].type = type2;
	// 	socket_para.sensor[1].value = value2;
	// 	socket_para.sensor[1].thrdLower = get_sensor_thrdlower(type2);
	// 	socket_para.sensor[1].thrdUpper = get_sensor_thrdupper(type2);
	// 	socket_para.sensor[1].range = get_sensor_range(type2);
	// 	attrSensor.changed = true;
	// 	check_sensor_config(1);
	// }
	// if (attrSensorAvailable.changed || attrSensor.changed) {
	// 	aliot_attr_post_changed();
	// }
}

/* ************************************************************************************************
 * 
 * attr toString && parse
 * 
 * ***********************************************************************************************/

ICACHE_FLASH_ATTR int getTimerString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
	if (user_socket_check_timer(ptmr) == TIMER_INVALID) {
		return os_sprintf(buf, "\"%s\":[]", attr->attrKey);
	}
	int i;
	os_sprintf(buf, KEY_FMT, attr->attrKey);
	os_sprintf(buf+os_strlen(buf), "%c", '[');
	for (i = 0; i < attr->spec.size; i++) {
		os_sprintf(buf + os_strlen(buf), "%d,", ptmr->array[i]);
	}
	int len = os_strlen(buf);
	buf[len-1] = ']';
	return len;
}

ICACHE_FLASH_ATTR static bool parseTimer(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsArray(result) == false) {
		return false;
	}
	if (cJSON_GetArraySize(result) != attr->spec.size) {
		return false;
	}
	uint8_t *buf = os_zalloc(attr->spec.size);
	if (buf == NULL) {
		return false;
	}
	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
	int i;
	for (i = 0; i < attr->spec.size; i++) {
		cJSON *item = cJSON_GetArrayItem(result, i);
		if (cJSON_IsNumber(item) == false) {
			os_free(buf);
			buf = NULL;
			return false;
		}
		buf[i] = item->valueint;
	}
	// enable
	if (buf[0] < 0 || buf[0] > 1) {
		return false;
	}
	// action
	if (buf[1] < ACTION_TURNOFF || buf[1] > ACTION_TURNON_DURATION) {
		return false;
	}
	// repeat
	if (buf[2] < 0 || buf[2] > 127) {
		return false;
	}
	// hour
	if (buf[3] < 0 || buf[3] > 23) {
		return false;
	}
	// minute
	if (buf[4] < 0 || buf[4] > 59) {
		return false;
	}
	// hour
	if (buf[5] < 0 || buf[5] > 59) {
		return false;
	}
	// end_hour
	if (buf[6] < 0 || buf[6] > 23) {
		return false;
	}
	// end_minute
	if (buf[7] < 0 || buf[7] > 59) {
		return false;
	}
	// end_hour
	if (buf[8] < 0 || buf[8] > 59) {
		return false;
	}
	os_memcpy(ptmr->array, buf, attr->spec.size);
	os_free(buf);
	buf = NULL;
	return true;
}

#define	SENSOR_FMT	"{\
\"Type\":%d,\
\"Value\":%d,\
\"ThrdLower\":%d,\
\"ThrdUpper\":%d\
},"
ICACHE_FLASH_ATTR int getSensorString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	sensor_t *psensor = (sensor_t *) attr->attrValue;
	int i;
	os_sprintf(buf, KEY_FMT, attr->attrKey);
	os_sprintf(buf+os_strlen(buf), "%c", '[');
	for (i = 0; i < attr->spec.size; i++) {
		if (check_sensor(i)) {
			os_sprintf(buf + os_strlen(buf), SENSOR_FMT, 
			psensor->type, psensor->value, psensor->thrdLower, psensor->thrdUpper);
			psensor++;
		} else {
			break;
		}
	}
	int len = os_strlen(buf);
	if (i == 0) {
		len += 1;
	}
	buf[len-1] = ']';
	return len;
}
// ICACHE_FLASH_ATTR int getSensorString(attr_t *attr, char *buf) {
// 	if (attrReadable(attr) == false) {
// 		return 0;
// 	}
// 	sensor_t *psensor = (sensor_t *) attr->attrValue;
// 	int i;
// 	uint64_t time = user_rtc_get_time();
// 	uint32_t v1 = time/100000000;
// 	uint32_t v2 = time%100000000;
// 	os_sprintf(buf, KEY_FMT, attr->attrKey);
// 	os_sprintf(buf+os_strlen(buf), "{\"time\":%d%d,\"value\":[", v1, v2);
// 	for (i = 0; i < attr->spec.size; i++) {
// 		if (check_sensor(i)) {
// 			os_sprintf(buf + os_strlen(buf), SENSOR_FMT, 
// 			psensor->type, psensor->value, psensor->thrdLower, psensor->thrdUpper);
// 			psensor++;
// 		} else {
// 			break;
// 		}
// 	}
// 	int len = os_strlen(buf);
// 	if (i == 0) {
// 		len += 1;
// 	}
// 	buf[len-1] = ']';
// 	buf[len] = '}';
// 	return len+1;
// }

#define	SENSOR_CONFIG_FMT	"{\
\"Type\":%d,\
\"NtfyEnable\":%d,\
\"NtfyLower\":%d,\
\"NtfyUpper\":%d,\
\"Day\":%d,\
\"Night\":%d\
},"
ICACHE_FLASH_ATTR int getSensorConfigString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	sensor_config_t *pcfg = (sensor_config_t *) attr->attrValue;
	int i;
	os_sprintf(buf, KEY_FMT, attr->attrKey);
	os_sprintf(buf+os_strlen(buf), "%c", '[');
	for (i = 0; i < attr->spec.size; i++) {
		if (check_sensor_config(i)) {
			os_sprintf(buf + os_strlen(buf), SENSOR_CONFIG_FMT, 
			pcfg->type, pcfg->ntfyEnable, pcfg->ntfyThrdLower, pcfg->ntfyThrdUpper, pcfg->dayConst, pcfg->nightConst);
			pcfg++;
		} else {
			break;
		}
	}
	int len = os_strlen(buf);
	if (i == 0) {
		len += 1;
	}
	buf[len-1] = ']';
	return len;
}

ICACHE_FLASH_ATTR bool parseSensorConfig(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsArray(result) == false) {
		return false;
	}
	int size = cJSON_GetArraySize(result);
	if (size > attr->spec.size) {
		return false;
	}
	int i;
	bool res = false;
	sensor_config_t *pcfg = (sensor_config_t *) attr->attrValue;
	for (i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(result, i);
		if (cJSON_IsObject(item) == false) {
			break;
		}
		cJSON *type = cJSON_GetObjectItem(item, "Type");
		cJSON *ntfyEnable = cJSON_GetObjectItem(item, "NtfyEnable");
		cJSON *ntfyLower = cJSON_GetObjectItem(item, "NtfyLower");
		cJSON *ntfyUpper = cJSON_GetObjectItem(item, "NtfyUpper");
		cJSON *day = cJSON_GetObjectItem(item, "Day");
		cJSON *night = cJSON_GetObjectItem(item, "Night");
		if (cJSON_IsNumber(type) == false || type->valueint == 0) {
			break;
		}
		if (cJSON_IsNumber(ntfyEnable) == false || ntfyEnable->valueint < 0 || ntfyEnable->valueint > 1) {
			break;
		}
		if (cJSON_IsNumber(ntfyLower) == false
			|| cJSON_IsNumber(ntfyUpper) == false
			|| cJSON_IsNumber(day) == false
			|| cJSON_IsNumber(night) == false) {
			break;
		}
		pcfg->type = type->valueint;
		pcfg->ntfyEnable = ntfyEnable->valueint;
		pcfg->ntfyThrdLower = ntfyLower->valueint;
		pcfg->ntfyThrdUpper = ntfyUpper->valueint;
		pcfg->dayConst = day->valueint;
		pcfg->nightConst = night->valueint;
		pcfg++;
		res = true;
	}
	size = i;
	for (i = size; i < attr->spec.size; i++) {
		os_memset(pcfg, 0x00, sizeof(sensor_config_t));
		pcfg++;
	}
	return res;
}

// #define	TIMER_FMT	"{\"Enable\":%d,\
// \"Action\":%d,\
// \"Repeat\":%d,\
// \"Hour\":%d,\
// \"Minute\":%d,\
// \"Second\":%d,\
// \"EndHour\":%d,\
// \"EndMinute\":%d,\
// \"EndSecond\":%d},"
// ICACHE_FLASH_ATTR static int getTimersString(attr_t *attr, char *buf) {
// 	if (attrReadable(attr) == false) {
// 		return 0;
// 	}
// 	int i;
// 	int size = 0;
// 	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
// 	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
// 		if (user_socket_check_timer(ptmr+i) == TIMER_INVALID) {
// 			size = i;
// 			break;
// 		}
// 	}
// 	if (size == 0) {
// 		return os_sprintf(buf, "\"%s\":[]", attr->attrKey);
// 	}
// 	os_sprintf(buf, KEY_FMT, attr->attrKey);
// 	os_sprintf(buf+os_strlen(buf), "%c", '[');
// 	for (i = 0; i < size; i++) {
// 		os_sprintf(buf+os_strlen(buf), TIMER_FMT, 
// 					ptmr->enable, ptmr->action, ptmr->repeat, 
// 					ptmr->hour, ptmr->minute, ptmr->second, 
// 					ptmr->end_hour, ptmr->end_minute, ptmr->end_second);
// 	}
// 	int len = os_strlen(buf);
// 	buf[len-1] = ']';
// 	return len;
// }
 
// ICACHE_FLASH_ATTR static bool parseTimers(attr_t *attr, cJSON *result) {
// 	if (attrWritable(attr) == false) {
// 		return false;
// 	}
// 	if (cJSON_IsArray(result) == false) {
// 		return false;
// 	}
// 	int size = cJSON_GetArraySize(result);
// 	if (size > attr->spec.size) {
// 		return false;
// 	}
// 	int i;
// 	bool res = false;
// 	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
// 	for (i = 0; i < size; i++) {
// 		cJSON *item = cJSON_GetArrayItem(result, i);
// 		if (cJSON_IsObject(item) == false) {
// 			break;
// 		}
// 		cJSON *enable = cJSON_GetObjectItem(item, "Enable");
// 		cJSON *action = cJSON_GetObjectItem(item, "Action");
// 		cJSON *repeat = cJSON_GetObjectItem(item, "Repeat");
// 		cJSON *hour = cJSON_GetObjectItem(item, "Hour");
// 		cJSON *minute = cJSON_GetObjectItem(item, "Minute");
// 		cJSON *second = cJSON_GetObjectItem(item, "Second");
// 		cJSON *endHour = cJSON_GetObjectItem(item, "EndHour");
// 		cJSON *endMinute = cJSON_GetObjectItem(item, "EndMinute");
// 		cJSON *endSecond = cJSON_GetObjectItem(item, "EndSecond");
// 		if (cJSON_IsNumber(enable) == false || enable->valueint < 0 || enable->valueint > 1) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(action) == false || action->valueint < ACTION_TURNOFF || action->valueint > ACTION_TURNON_DURATION) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(repeat) == false || repeat->valueint < 0 || repeat->valueint > 127) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(hour) == false || hour->valueint < 0 || hour->valueint > 23) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(minute) == false || minute->valueint < 0 || minute->valueint > 59) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(second) == false || second->valueint < 0 || second->valueint > 59) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(endHour) == false || endHour->valueint < 0 || endHour->valueint > 23) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(endMinute) == false || endMinute->valueint < 0 || endMinute->valueint > 59) {
// 			break;
// 		}
// 		if (cJSON_IsNumber(endSecond) == false || endSecond->valueint < 0 || endSecond->valueint > 59) {
// 			break;
// 		}
// 		ptmr->enable = enable->valueint;
// 		ptmr->action = action->valueint;
// 		ptmr->repeat = repeat->valueint;
// 		ptmr->hour = hour->valueint;
// 		ptmr->minute = minute->valueint;
// 		ptmr->second = second->valueint;
// 		ptmr->end_hour = endHour->valueint;
// 		ptmr->end_minute = endMinute->valueint;
// 		ptmr->end_second = endSecond->valueint;
// 		ptmr++;
// 		res = true;
// 	}
// 	size = i;
// 	for (i = size; i < SOCKET_TIMER_MAX; i++) {
// 		os_memset(ptmr, 0xFF, sizeof(socket_timer_t));
//		ptmr++;
// 	}
// 	return res;
// }
