#ifndef	_USER_MONSOON_H_
#define	_USER_MONSOON_H_

#include "user_device.h"

#define	MONSOON_TIMER_MAX			24				//定时器个数

#define	CUSTOM_COUNT				8				//自定义喷水动作个数

#define SPRAY_OFF					0				//喷水关闭
#define SPRAY_MIN					1				//喷水最小时长
#define SPRAY_DEFAULT				5				//喷水默认时长
#define	SPRAY_MAX					120				//喷水最长时间

// typedef struct {
// 	bool enable;
// 	uint8_t repeat;
// 	uint8_t period;
// 	uint8_t hour;
// 	uint8_t minute;
// } monsoon_timer_t;

typedef union {
	struct {
		unsigned timer : 17;
		unsigned period : 7;
		unsigned repeat : 7;
		unsigned enable : 1;
	};
	int value;
} monsoon_timer_t;

typedef struct {
	device_config_t super;

	int key_action;
	int custom_actions[CUSTOM_COUNT];
	monsoon_timer_t timers[MONSOON_TIMER_MAX];
} monsoon_config_t;

typedef struct {
	int power;
} monsoon_para_t;

extern	user_device_t user_dev_monsoon;

#endif