#ifndef	_USER_SOCKET_H_
#define	_USER_SOCKET_H_

#include "user_device.h"

#define	CONNECT_DEVICE_NAME_LEN		64

#define SOCKET_TIMER_MAX			24

#define POWER_ON					1
#define POWER_OFF					0

#define	SWITCH_COUNT_MAX			50000

#define	ACTION_TURNOFF				0
#define	ACTION_TURNON				1
#define	ACTION_TURNON_DURATION		2

#define	MODE_TIMER					0
#define	MODE_SENSOR1				1
#define	MODE_SENSOR2				2

#define	SENSOR_COUNT_MAX			2
#define	SENSOR_NAME_LEN				32
#define	SENSOR_UNIT_LEN				16

#define	SENSOR_TEMPERATURE			1
#define	SENSOR_HUMIDITY				2

#define	TEMPERATURE_RANGE			2
#define	HUMIDITY_RANGE				5

#define	TEMPERATURE_THRDLOWER		10
#define	TEMPERATURE_THRDUPPER		40

#define	HUMIDITY_THRDLOWER			20
#define	HUMIDITY_THRDUPPER			80

typedef struct {
	uint8_t type;					// 传感器类型
	// char name[SENSOR_NAME_LEN];		// 传感器名称
	// char unit[SENSOR_UNIT_LEN];		// 传感器单位
	int value;						// 数值
	int thrdLower;					// 参数取值下限
	int thrdUpper;					// 参数取值上限
	uint8_t range;					// 恒定控制值范围
} sensor_t;

typedef struct {
	uint8_t type;					// 传感器类型
	bool ntfyEnable;				// 通知使能
	int ntfyThrdLower;				// 低于下限通知
	int ntfyThrdUpper;				// 高于上限通知
	int dayConst;					// 白天恒定控制值
	int nightConst;					// 夜晚恒定控制值
} sensor_config_t;

typedef union {
	struct {
		bool	enable;
		uint8_t action;
		uint8_t repeat;
		uint8_t hour;
		uint8_t minute;
		uint8_t second;
		uint8_t end_hour;
		uint8_t end_minute;
		uint8_t end_second;
		uint8_t year;
		uint8_t month;
		uint8_t day;
	};
	uint8_t array[12];
} socket_timer_t;

typedef struct {
	device_config_t super;

	int switch_flag;
	int switch_count;

	int mode;						// 插座工作模式
	
	socket_timer_t timers[SOCKET_TIMER_MAX];
	sensor_config_t sensor_config[SENSOR_COUNT_MAX];
} socket_config_t;

typedef struct {
	bool power;
	
	bool sensor_available;
	sensor_t sensor[SENSOR_COUNT_MAX];
} socket_para_t;

extern	user_device_t user_dev_socket;

#endif