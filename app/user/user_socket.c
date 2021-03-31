#include "user_socket.h"
#include "app_board_socket.h"
#include "app_test.h"
#include "aliot_attr.h"
#include "user_rtc.h"
#include "user_uart.h"

#define	REGION_SOCKET			"us-west-1"
#define	PRODUCT_KEY_SOCKET		"a3pXBGXhUbn"
#define	PRODUCT_SECRET_SOCKET	"ko1ucDsDztil8AHY"

// length must < 25
#define	PRODUCT_NAME			"ExoSocket"

#define	KEY_NUM					1

#define	SWITCH_SAVED_FLAG		0x12345678

#define	LINKAGE_LOCK_PERIOD		60000		//lock linkage after manual turnon/turnoff
#define	LINKAGE_LOCKON_PERIOD	900000		//lock socket for 15min after turnon

#define FRAME_HEADER			0x68
#define CMD_SET					0x00
#define CMD_GET					0x01

typedef enum {
	POWEROFF_NOCHANGE,
	POWERON_NOCHANGE,
	POWEROFF_CHANGED,
	POWERON_CHANGED
} power_state_t;

typedef enum _day_night { 
	DAY, 
	NIGHT 
} day_night_t;

static void user_socket_update_timers();

static void user_socket_detect_sensor(void *arg);
static void user_socket_decode_sensor(uint8_t *pbuf, uint8_t len);
static void sensor_linkage_process(day_night_t day_night);

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

// static int 	getTimerString(aliot_attr_t *attr, char *text);
static cJSON* 	getTimerJson(aliot_attr_t *attr);
static bool parseTimer(aliot_attr_t *attr, cJSON *result);
// static int 	getSensorString(aliot_attr_t *attr, char *text);
static cJSON* 	getSensorJson(aliot_attr_t *attr);
// static int 	getSensorConfigString(aliot_attr_t *attr, char *text);
static cJSON* 	getSensorConfigJson(aliot_attr_t *attr);
static bool parseSensorConfig(aliot_attr_t *attr, cJSON *result);

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

socket_para_t socket_para;
user_device_t user_dev_socket = {
	.meta = {
		.region				= REGION_SOCKET,
		.product_key		= PRODUCT_KEY_SOCKET,
		.product_secret		= PRODUCT_SECRET_SOCKET,
	},
	.product = PRODUCT_NAME,
	.firmware_version 	= FIRMWARE_VERSION,

	.key_io_num = KEY_IO_NUM,
	.test_led1_num = LEDR_IO_NUM,
	.test_led2_num = LEDG_IO_NUM,

	.board_init = app_board_socket_init,
	.init = user_socket_init,
	.process = user_socket_process,
	.attr_set_cb = user_socket_attr_set_cb,

	.attrDeviceInfo = newAttr("DeviceInfo", &user_dev_socket.dev_info, NULL, &deviceInfoVtable),
	.attrFirmwareVersion = newIntAttr("FirmwareVersion", (int *) &user_dev_socket.firmware_version, 1, 65535, &rdIntVtable),
	.attrZone = newIntAttr("Zone", &socket_config.super.zone, -720, 720, &defIntVtable),
	.attrDeviceTime = newTextAttr("DeviceTime", user_dev_socket.device_time, sizeof(user_dev_socket.device_time), &rdTextVtable),
	.attrSunrise = newIntAttr("Sunrise", &socket_config.super.sunrise, 0, 1439, &defIntVtable),
	.attrSunset = newIntAttr("Sunset", &socket_config.super.sunset, 0, 1439, &defIntVtable)
};

static const int switch_max = SWITCH_COUNT_MAX;

static const attr_vtable_t timerAttrVtable = newAttrVtable(getTimerJson, parseTimer);
static const attr_vtable_t sensorAttrVtable = newReadAttrVtable(getSensorJson);
static const attr_vtable_t sensorConfigAttrVtable = newAttrVtable(getSensorConfigJson, parseSensorConfig);

static aliot_attr_t attrSwitchMax = newIntAttr("SwitchMax", (int *) &switch_max, 0, 1000000, &rdIntVtable);
static aliot_attr_t attrSwitchCount = newIntAttr("SwitchCount", &socket_config.switch_count, 0, 1000000, &defIntVtable);
static aliot_attr_t attrMode = newIntAttr("Mode", &socket_config.mode, MODE_TIMER, MODE_SENSOR2, &defIntVtable);
static aliot_attr_t attrPower = newBoolAttr("Power", &socket_para.power, &defBoolVtable);
static aliot_attr_t attrTimers[SOCKET_TIMER_MAX] = {
	newArrayAttr("T1", &socket_config.timers[0], 9, &timerAttrVtable),
	newArrayAttr("T2", &socket_config.timers[1], 9, &timerAttrVtable),
	newArrayAttr("T3", &socket_config.timers[2], 9, &timerAttrVtable),
	newArrayAttr("T4", &socket_config.timers[3], 9, &timerAttrVtable),
	newArrayAttr("T5", &socket_config.timers[4], 9, &timerAttrVtable),
	newArrayAttr("T6", &socket_config.timers[5], 9, &timerAttrVtable),
	newArrayAttr("T7", &socket_config.timers[6], 9, &timerAttrVtable),
	newArrayAttr("T8", &socket_config.timers[7], 9, &timerAttrVtable),
	newArrayAttr("T9", &socket_config.timers[8], 9, &timerAttrVtable),
	newArrayAttr("T10", &socket_config.timers[9], 9, &timerAttrVtable),
	newArrayAttr("T11", &socket_config.timers[10], 9, &timerAttrVtable),
	newArrayAttr("T12", &socket_config.timers[11], 9, &timerAttrVtable),
	newArrayAttr("T13", &socket_config.timers[12], 9, &timerAttrVtable),
	newArrayAttr("T14", &socket_config.timers[13], 9, &timerAttrVtable),
	newArrayAttr("T15", &socket_config.timers[14], 9, &timerAttrVtable),
	newArrayAttr("T16", &socket_config.timers[15], 9, &timerAttrVtable),
	newArrayAttr("T17", &socket_config.timers[16], 9, &timerAttrVtable),
	newArrayAttr("T18", &socket_config.timers[17], 9, &timerAttrVtable),
	newArrayAttr("T19", &socket_config.timers[18], 9, &timerAttrVtable),
	newArrayAttr("T20", &socket_config.timers[19], 9, &timerAttrVtable),
	newArrayAttr("T21", &socket_config.timers[20], 9, &timerAttrVtable),
	newArrayAttr("T22", &socket_config.timers[21], 9, &timerAttrVtable),
	newArrayAttr("T23", &socket_config.timers[22], 9, &timerAttrVtable),
	newArrayAttr("T24", &socket_config.timers[23], 9, &timerAttrVtable)
};
static aliot_attr_t attrSensorAvailable = newBoolAttr("SensorAvailable", &socket_para.sensor_available, &rdBoolVtable);
static aliot_attr_t attrSensor = newArrayAttr("Sensor", &socket_para.sensor[0], SENSOR_COUNT_MAX, &sensorAttrVtable);
static aliot_attr_t attrSensorConfig = newArrayAttr("SensorConfig", &socket_config.sensor_config[0], SENSOR_COUNT_MAX, &sensorConfigAttrVtable);

ICACHE_FLASH_ATTR static void user_socket_attr_init() {
	uint8_t i;

	aliot_attr_add(&user_dev_socket.attrDeviceInfo);
	aliot_attr_add(&user_dev_socket.attrFirmwareVersion);
	aliot_attr_add(&user_dev_socket.attrZone);
	aliot_attr_add(&user_dev_socket.attrDeviceTime);
	aliot_attr_add(&user_dev_socket.attrSunrise);
	aliot_attr_add(&user_dev_socket.attrSunset);

	aliot_attr_add(&attrSwitchMax);
	aliot_attr_add(&attrSwitchCount);
	aliot_attr_add(&attrMode);
	aliot_attr_add(&attrPower);
	for (i = 0; i < SOCKET_TIMER_MAX; i++) {
		aliot_attr_add(&attrTimers[i]);
	}
	aliot_attr_add(&attrSensorAvailable);
	aliot_attr_add(&attrSensor);
	aliot_attr_add(&attrSensorConfig);
}

/**************************************************************************************************
 * 
 * SmartConfig & APConfig
 * 
 * ***********************************************************************************************/

/**
 * @param zone: -720 ~ 720
 * */
ICACHE_FLASH_ATTR static void user_socket_settime(int zone, uint64_t time) {
	user_rtc_set_time(time);
	socket_config.super.zone = zone;
	user_dev_socket.attrZone.changed = true;

	user_socket_update_timers();
	user_socket_save_config();
}

ICACHE_FLASH_ATTR static void user_socket_ledg_toggle() {
	ledg_toggle();
}

ICACHE_FLASH_ATTR static void user_socket_pre_smartconfig() {
	ledall_off();
	aliot_mqtt_disconnect();
	user_indicator_start(SMARTCONFIG_FLASH_PERIOD, 0, user_socket_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_socket_post_smartconfig() {
	user_indicator_stop();
	ledg_off();
	if (relay_status()) {
		ledrelay_on();
	} else {
		ledrelay_off();
	}
}

ICACHE_FLASH_ATTR static void user_socket_pre_apconfig() {
	ledall_off();
	aliot_mqtt_disconnect();
	user_indicator_start(APCONFIG_FLASH_PERIOD, 0, user_socket_ledg_toggle);
}

ICACHE_FLASH_ATTR static void user_socket_post_apconfig() {
	user_indicator_stop();
	ledg_off();
	if (relay_status()) {
		ledrelay_on();
	} else {
		ledrelay_off();
	}
}

/*************************************************************************************************/

ICACHE_FLASH_ATTR static void user_socket_linkage_lockon_cb(void *arg) {
	mLockon = false;
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lockon_start() {
	os_timer_disarm(&linkage_lockon_tmr);
	os_timer_setfn(&linkage_lockon_tmr, user_socket_linkage_lockon_cb, NULL);
	mLockon = true;
	os_timer_arm(&linkage_lockon_tmr, LINKAGE_LOCKON_PERIOD, 0);
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lock_cb(void *arg) {
	mLock = false;
}

ICACHE_FLASH_ATTR static void user_socket_linkage_lock_start() {
	os_timer_disarm(&linkage_lock_tmr);
	os_timer_setfn(&linkage_lock_tmr, user_socket_linkage_lock_cb, NULL);
	mLock = true;
	os_timer_arm(&linkage_lock_tmr, LINKAGE_LOCK_PERIOD, 0);
}

ICACHE_FLASH_ATTR static power_state_t user_socket_power_update() {
	if (socket_para.power) {
		if (relay_status()) {
			return POWERON_NOCHANGE;
		}
		relay_on();
		ledg_on();
		socket_config.switch_flag	= SWITCH_SAVED_FLAG;
		socket_config.switch_count++;
		attrSwitchCount.changed		= true;
		return POWERON_CHANGED;
	}
	if (relay_status()) {
		relay_off();
		ledg_off();
		return POWEROFF_CHANGED;
	}
	return POWEROFF_NOCHANGE;
}

/* ***********************************************************************************************
 * 
 * key functions
 * 
 * ***********************************************************************************************/

ICACHE_FLASH_ATTR static void user_socket_key_short_press_cb() {	
	if (user_smartconfig_status() || user_apconfig_status()) {
		return;
	}
	
	socket_para.power = !socket_para.power;
	if (user_socket_power_update() == POWERON_CHANGED) {
		user_socket_save_config();
	}
	user_socket_linkage_lock_start();

	attrPower.changed = true;
	user_device_post_changed();
}

ICACHE_FLASH_ATTR static void user_socket_key_long_press_cb() {
	if (app_test_status()) {				//测试模式
		return;	
	}
	if (user_smartconfig_status()) {
		user_smartconfig_stop();
		user_apconfig_start(user_dev_socket.apssid, APCONFIG_TIMEOUT, user_socket_pre_apconfig, user_socket_post_apconfig, user_socket_settime);
	} else if (user_apconfig_status()) {
		return;
	} else {
		user_smartconfig_start(SMARTCONFIG_TIEMOUT, user_socket_pre_smartconfig, user_socket_post_smartconfig);
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

ICACHE_FLASH_ATTR static void date_inc_day(uint16_t *year, uint8_t *month, uint8_t *day) {
	uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if ((*month) == 2) {
		if ((*year)%400 == 0 || ((*year)%4 == 0 && (*year)%100 !=0)) {
			mdays[1] = 29;
		}
	}
	if ((*day) >= mdays[(*month)-1]) {
		*day = 1;
		(*month)++;
		if ((*month) > 12) {
			*month = 1;
			(*year)++;
		}
	} else {
		(*day)++;
	}
}

ICACHE_FLASH_ATTR static bool user_socket_check_timer(socket_timer_t *ptmr) {
	if (ptmr == NULL  
		|| ptmr->enable > 1 || ptmr->repeat > 0x7F || ptmr->action > ACTION_TURNON_DURATION 
		|| ptmr->hour > 23 || ptmr->minute > 59 || ptmr->second > 59
		|| ptmr->end_hour > 23 || ptmr->end_minute > 59 || ptmr->end_second > 59) {
		return false;
	}
	if (ptmr->action == ACTION_TURNON_DURATION
		&& ptmr->hour == ptmr->end_hour
		&& ptmr->minute == ptmr->end_minute
		&& ptmr->second == ptmr->end_second) {
		return false;
	}
	if (ptmr->enable) {
		if (ptmr->repeat == 0) {
			if (ptmr->month == 0 || ptmr->month > 12) {
				return false;
			}
			uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
			if (ptmr->month == 2) {
				if (ptmr->year%4 == 0 && ptmr->year%100 != 0) {
					mdays[1] = 29;
				}
			}
			if (ptmr->day == 0 || ptmr->day > mdays[ptmr->month-1]) {
				return false;
			}
		}
	}
	return true;
}

ICACHE_FLASH_ATTR static void user_socket_reset_timer(socket_timer_t *ptmr) {
	if (ptmr == NULL) {
		return;
	}
	os_memset(ptmr, 0xFF, sizeof(socket_timer_t));
}

/**
 * @brief Get the total days from 1970.1.1
 * 
 * @param year 
 * @param month 
 * @param day 
 * @return uint32_t 
 */
ICACHE_FLASH_ATTR uint32_t get_total_days(uint16_t year, uint8_t month, uint8_t day) {
	if (year < EPOCH_YEAR 
		|| month == 0 || month > 12
		|| day == 0 || day > 31) {
		return 0;
	}
	uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month == 2 && user_rtc_leapyear(year)) {
		mdays[1] = 29;
	}

	uint32_t total_days = 0;
	for (int i = EPOCH_YEAR; i < year; i++) {
		if (user_rtc_leapyear(i)) {
			total_days += 366;
		} else {
			total_days += 365;
		}
	}
	for (int i = 1; i < month; i++) {
		total_days += mdays[i-1];
	}
	total_days += day;
	return total_days;
}

/**
 * @brief 
 * 
 * @param ptmr 
 * @return true 	timer updated
 * @return false 	timer do not need update
 */
ICACHE_FLASH_ATTR static bool update_timer(socket_timer_t *ptmr) {
	if (ptmr->enable && ptmr->repeat == 0) {
		uint64_t total_days = get_total_days(ptmr->year+2000, ptmr->month, ptmr->day);

		uint64_t cur_time = user_rtc_get_time()/1000 + socket_config.super.zone*60;
		uint64_t act_time = ptmr->hour*3600 + ptmr->minute*60 + ptmr->second;
		if (ptmr->action == ACTION_TURNON_DURATION) {
			uint64_t end_time = ptmr->end_hour*3600 + ptmr->end_minute*60 + ptmr->end_second;
			if (end_time < act_time) {
				total_days += 1;
			}
			end_time += total_days*SECONDS_PER_DAY;
			if (end_time < cur_time) {
				ptmr->enable = false;
				return true;
			}
		} else {
			act_time += total_days*SECONDS_PER_DAY;
			if (act_time < cur_time) {
				ptmr->enable = false;
				return true;
			}
		}
	}
	return false;
}

static void user_socket_update_timers() {
	uint8_t cnt = SOCKET_TIMER_MAX;
	socket_timer_t *ptmr;
	bool flag = false;
	for (int i = 0; i < SOCKET_TIMER_MAX; i++) {
		ptmr = &socket_config.timers[i];
		if (user_socket_check_timer(ptmr) == false) {
			cnt	= i;
			break;
		}
		if (update_timer(ptmr)) {
			flag = true;
		}
	}

	for(int i = cnt; i < SOCKET_TIMER_MAX; i++) {
		memset(&socket_config.timers[i], 0xFF, sizeof(socket_timer_t));
	}
	if (flag) {
		user_socket_save_config();
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
}

ICACHE_FLASH_ATTR static void user_socket_init() {
	ledrelay_off();
	uart0_init(BAUDRATE_9600, 16);
	uart0_set_rx_cb(user_socket_decode_sensor);
	uart_enable_isr();

	user_socket_para_init();
	user_socket_key_init();
	user_socket_attr_init();
	os_timer_disarm(&sensor_detect_tmr);
	os_timer_setfn(&sensor_detect_tmr, user_socket_detect_sensor, NULL);
	os_timer_arm(&sensor_detect_tmr, 32, 1);
}

ICACHE_FLASH_ATTR static void user_socket_process(void *arg) {
	if (user_smartconfig_status() || user_apconfig_status()) {
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

	if ((socket_config.mode == MODE_SENSOR1 || socket_config.mode == MODE_SENSOR2) && socket_para.sensor_available) {
		day_night_t day_night = NIGHT;
		uint16_t sunrise = socket_config.super.sunrise;
		uint16_t sunset = socket_config.super.sunset;
		uint16_t ct = datetime.hour*60 + datetime.minute;
		if (sunrise < sunset && (ct >= sunrise && ct < sunset)) {
			day_night = DAY;
		} else if (sunrise > sunset && (ct >= sunrise || ct < sunset)) {
			day_night = DAY;
		}
		sensor_linkage_process(day_night);
		return;
	}
	last_linkage_idx = -1;

	bool flag = false;
	if (socket_config.mode != MODE_TIMER) {
		socket_config.mode	= MODE_TIMER;
		attrMode.changed = true;
		flag	= true;
	}

	uint32_t cur_time = datetime.hour*3600 + datetime.minute*60 + datetime.second;
	uint8_t year = datetime.year - 2000;
	socket_timer_t *p;
	
	bool save = false;
	bool action = false;
	uint8_t week = datetime.weekday;
	
	for (int i = 0; i < SOCKET_TIMER_MAX; i++) {
		p = &socket_config.timers[i];
		if (user_socket_check_timer(p) == false) {
			break;
		}
		if (p->enable == false) {
			continue;
		}
		uint32_t act_time = p->hour*3600 + p->minute*60 + p->second;
		if (act_time == cur_time) {
			if (p->repeat == 0) {
				if (p->year == year && p->month == datetime.month && p->day == datetime.day) {
					action = (p->action > ACTION_TURNOFF ? true : false);
					flag = true;
					if (p->action != ACTION_TURNON_DURATION) {
						p->enable = 0;
						attrTimers[i].changed = true;
						save = true;
					}
				}
			} else if ((p->repeat&(1<<week)) != 0) {
				action = (p->action > ACTION_TURNOFF ? true : false);
				flag = true;
			}
		} else if (p->action == ACTION_TURNON_DURATION) {
			uint32_t end_time = p->end_hour*3600 + p->end_minute*60 + p->end_second;
			if (end_time == cur_time) {
				if (p->repeat == 0) {
					if (act_time > end_time) {
						date_inc_day(&datetime.year, &datetime.month, &datetime.day);
					}
					year = datetime.year - 2000;
					if (p->year == year && p->month == datetime.month && p->day == datetime.day) {
						p->enable = false;
						action = false;
						flag = true;
						attrTimers[i].changed = true;
						save = true;
					}
				} else {
					if (act_time > end_time) {
						week++;
						if (week > 6) {
							week = 0;
						}
					}
					if ((p->repeat&(1<<week)) != 0) {
						action = false;
						flag = true;
					}
				}
			}
		}
	}
	if (flag) {
		socket_para.power = action;
		if (user_socket_power_update() == POWERON_CHANGED) {
			save = true;
		}
		if(save) {
			user_socket_save_config();
		}
		attrPower.changed = true;
		user_device_post_changed();
	}
}

/**
 * @param idx: 传感器索引
 * @param day_night: DAY or NIGHT
 * */
ICACHE_FLASH_ATTR static void sensor_linkage_process(day_night_t day_night) {
	static uint8_t last_mode = MODE_TIMER;
	static uint8_t overCount;
	static uint8_t lossCount;
	static bool overFlag;
	static bool lossFlag;

	if (last_mode != socket_config.mode) {
		overCount = 0;
		lossCount = 0;
		overFlag = false;
		lossFlag = false;

		last_mode = socket_config.mode;
	}

	uint8_t	idx = socket_config.mode-1;
	sensor_t *psensor = &socket_para.sensor[idx];
	sensor_config_t *pcfg = &socket_config.sensor_config[idx];
	uint8_t range = psensor->range;

	int32_t target = (day_night == DAY ? pcfg->dayConst : pcfg->nightConst);
	int32_t max = (target+range)*10;
	int32_t min = (target-range)*10;
	int32_t temp = psensor->value;

	/* check sensor over flag */
	if (overFlag == false) {
		if (temp > max) {
			overCount++;
			if (overCount > 60) {
				overFlag = true;
				overCount = 0;

				LOGD(TAG, "sensor%d overflag: 1", (idx+1));
			}
		} else {
			overCount = 0;
		}
	} else {
		if (temp < max) {
			overCount++;
			if (overCount > 60) {
				overFlag = false;
				overCount = 0;

				LOGD(TAG, "sensor%d overflag: 0", (idx+1));
			}
		} else {
			overCount = 0;
		}
	}

	/* check sensor loss flag */
	if (lossFlag == false) {
		if (temp < min) {
			lossCount++;
			if (lossCount > 60) {
				lossFlag = true;
				lossCount = 0;

				LOGD(TAG, "sensor%d lossflag: 1", (idx+1));
			}
		} else {
			lossCount = 0;
		}
	} else {
		if (temp > min) {
			lossCount++;
			if (lossCount > 60) {
				lossFlag = false;
				lossCount = 0;

				LOGD(TAG, "sensor%d lossflag: 0", (idx+1));
			}
		} else {
			lossCount = 0;
		}
	}

	// LOGD(TAG, "val: %d min: %d max: %d over1: %d loss1: %d over2: %d loss2: %d", 
	// 	temp, min, max, overFlag[0], lossFlag[0], overFlag[1], lossFlag[1]);

	if (mLock) {
		return;
	}

	if (overFlag) {
		socket_para.power = false;
		if (user_socket_power_update() == POWEROFF_CHANGED) {
			attrPower.changed = true;
			user_device_post_changed();
		}
		return;
	}
	if (lossFlag && !mLockon) {
		socket_para.power = true;
		if (user_socket_power_update() == POWERON_CHANGED) {
			attrPower.changed = true;
			user_device_post_changed();
			user_socket_save_config();
			user_socket_linkage_lockon_start();
		}
	}
}

ICACHE_FLASH_ATTR static void user_socket_attr_set_cb() {
	power_state_t state = user_socket_power_update();
	if (state == POWERON_CHANGED || state == POWEROFF_CHANGED) {
		user_socket_linkage_lock_start();
	}

	bool flag = false;
	do {
		if (user_dev_socket.attrZone.changed) {
			flag = true;
			break;
		}
		for (int i = 0; i < SOCKET_TIMER_MAX; i++) {
			if (attrTimers[i].changed) {
				flag = true;
				break;
			}
		}
	} while (0);
	if (flag) {
		user_socket_update_timers();
	}

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

		user_device_post_changed();
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
		user_device_post_changed();
	}
}

/* ************************************************************************************************
 * 
 * attr toString && parse
 * 
 * ***********************************************************************************************/

static cJSON* getTimerJson(aliot_attr_t *attr) {
	cJSON *result = cJSON_CreateArray();
	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
	if (user_socket_check_timer(ptmr) == false) {
		return result;
	}
	
	for (int i = 0; i < attr->spec.size; i++) {
		cJSON *item = cJSON_CreateNumber(ptmr->array[i]);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

// ICACHE_FLASH_ATTR int getTimerString(aliot_attr_t *attr, char *buf) {
// 	if (attrReadable(attr) == false) {
// 		return 0;
// 	}
// 	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
// 	if (user_socket_check_timer(ptmr) == false) {
// 		return os_sprintf(buf, "\"%s\":[]", attr->attrKey);
// 	}
// 	int i;
// 	os_sprintf(buf, KEY_FMT, attr->attrKey);
// 	os_sprintf(buf+os_strlen(buf), "%c", '[');
// 	for (i = 0; i < attr->spec.size; i++) {
// 		os_sprintf(buf + os_strlen(buf), "%d,", ptmr->array[i]);
// 	}
// 	int len = os_strlen(buf);
// 	buf[len-1] = ']';
// 	return len;
// }

ICACHE_FLASH_ATTR static bool parseTimer(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsArray(result) == false) {
		return false;
	}
	socket_timer_t *ptmr = (socket_timer_t *) attr->attrValue;
	int size = cJSON_GetArraySize(result);
	if (size == 0) {
		memset(ptmr, 0xFF, sizeof(socket_timer_t));
		return true;
	}
	if (size != attr->spec.size) {
		return false;
	}
	socket_timer_t *temp = (socket_timer_t *) os_zalloc(sizeof(socket_timer_t));
	if (temp == NULL) {
		LOGI(TAG, "zalloc temp failed");
		return false;
	}
	for (int i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(result, i);
		if (cJSON_IsNumber(item) == false || item->valueint < 0 || item->valueint > 0xFF) {
			os_free(temp);
			temp = NULL;
			return false;
		}
		temp->array[i] = item->valueint;
	}

	if (temp->enable && temp->repeat == 0) {
		date_time_t datetime = {0};
		if (user_rtc_get_datetime(&datetime, socket_config.super.zone)) {
			uint32_t act_time = temp->hour*3600 + temp->minute*60 + temp->second;
			uint32_t cur_time = datetime.hour*3600 + datetime.minute*60 + datetime.second;
			if (act_time <= cur_time) {
				date_inc_day(&datetime.year, &datetime.month, &datetime.day);
			}
			temp->year = datetime.year-2000;
			temp->month = datetime.month;
			temp->day = datetime.day;
		}
	}
	if (user_socket_check_timer(temp) == false) {
		os_free(temp);
		temp = NULL;
		return false;
	}
	
	os_memcpy(ptmr, temp, sizeof(socket_timer_t));

	os_free(temp);
	temp = NULL;

	return true;
}

static cJSON* getSensorJson(aliot_attr_t *attr) {
	if (socket_para.sensor_available == false) {
		return NULL;
	}
	cJSON *result = cJSON_CreateArray();
	sensor_t *psensor = (sensor_t *) attr->attrValue;
	for (int idx = 0; idx < attr->spec.size; idx++) {
		if (psensor->type == 0) {
			break;
		}
		cJSON *item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "Type", psensor->type);
		cJSON_AddNumberToObject(item, "Value", psensor->value);
		cJSON_AddNumberToObject(item, "Min", psensor->thrdLower);
		cJSON_AddNumberToObject(item, "Max", psensor->thrdUpper);
		cJSON_AddItemToArray(result, item);
		psensor++;
	}
	return result;
}

// #define	SENSOR_FMT	"{
// \"Type\":%d,
// \"Value\":%d,
// \"Min\":%d,
// \"Max\":%d
// },"
// ICACHE_FLASH_ATTR int getSensorString(aliot_attr_t *attr, char *buf) {
// 	if (attrReadable(attr) == false) {
// 		return 0;
// 	}
// 	sensor_t *psensor = (sensor_t *) attr->attrValue;
// 	int i;
// 	os_sprintf(buf, KEY_FMT, attr->attrKey);
// 	os_sprintf(buf+os_strlen(buf), "%c", '[');
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
// 		os_memset(buf, 0, len);
// 		len = 0;
// 	} else {
// 		buf[len-1] = ']';
// 	}
// 	return len;
// }

static cJSON* getSensorConfigJson(aliot_attr_t *attr) {
	if (socket_para.sensor_available == false) {
		return NULL;
	}
	cJSON *result = cJSON_CreateArray();
	sensor_config_t *pcfg = (sensor_config_t *) attr->attrValue;
	for (int idx = 0; idx < attr->spec.size; idx++) {
		if (check_sensor_config(idx)) {
			cJSON *item = cJSON_CreateObject();
			cJSON_AddNumberToObject(item, "Type", pcfg->type);
			cJSON_AddNumberToObject(item, "Ntfy", pcfg->ntfyEnable);
			cJSON_AddNumberToObject(item, "Lower", pcfg->ntfyThrdLower);
			cJSON_AddNumberToObject(item, "Upper", pcfg->ntfyThrdUpper);
			cJSON_AddNumberToObject(item, "Day", pcfg->dayConst);
			cJSON_AddNumberToObject(item, "Night", pcfg->nightConst);
			pcfg++;
		} else {
			break;
		}
	}
	return result;
}

// #define	SENSOR_CONFIG_FMT	"{
// \"Type\":%d,
// \"Ntfy\":%d,
// \"Lower\":%d,
// \"Upper\":%d,
// \"Day\":%d,
// \"Night\":%d
// },"
// ICACHE_FLASH_ATTR int getSensorConfigString(aliot_attr_t *attr, char *buf) {
// 	if (attrReadable(attr) == false) {
// 		return 0;
// 	}
// 	sensor_config_t *pcfg = (sensor_config_t *) attr->attrValue;
// 	int i;
// 	os_sprintf(buf, KEY_FMT, attr->attrKey);
// 	os_sprintf(buf+os_strlen(buf), "%c", '[');
// 	for (i = 0; i < attr->spec.size; i++) {
// 		if (check_sensor_config(i)) {
// 			os_sprintf(buf + os_strlen(buf), SENSOR_CONFIG_FMT, 
// 			pcfg->type, pcfg->ntfyEnable, pcfg->ntfyThrdLower, pcfg->ntfyThrdUpper, pcfg->dayConst, pcfg->nightConst);
// 			pcfg++;
// 		} else {
// 			break;
// 		}
// 	}
// 	int len = os_strlen(buf);
// 	if (i == 0) {
// 		os_memset(buf, 0, len);
// 		len = 0;
// 	} else {
// 		buf[len-1] = ']';
// 	}
// 	return len;
// }

ICACHE_FLASH_ATTR bool parseSensorConfig(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsArray(result) == false) {
		return false;
	}
	int size = cJSON_GetArraySize(result);
	if (size == 0 || size > attr->spec.size) {
		return false;
	}
	int idx;
	bool ret = false;
	sensor_config_t *pcfg = (sensor_config_t *) attr->attrValue;
	for (idx = 0; idx < size; idx++) {
		cJSON *item = cJSON_GetArrayItem(result, idx);
		if (cJSON_IsObject(item) == false) {
			break;
		}
		cJSON *type = cJSON_GetObjectItem(item, "Type");
		cJSON *ntfyEnable = cJSON_GetObjectItem(item, "Ntfy");
		cJSON *ntfyLower = cJSON_GetObjectItem(item, "Lower");
		cJSON *ntfyUpper = cJSON_GetObjectItem(item, "Upper");
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
		ret = true;
	}
	for (int i = idx; i < attr->spec.size; i++) {
		os_memset(pcfg, 0x00, sizeof(sensor_config_t));
		pcfg++;
	}
	return ret;
}
