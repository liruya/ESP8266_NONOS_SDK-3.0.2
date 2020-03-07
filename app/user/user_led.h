#ifndef	_USER_LED_H_
#define	_USER_LED_H_

#include "user_device.h"

#define	CHANNEL_COUNT_MIN		1
#define	CHANNEL_COUNT_MAX		6
#define	CHANNEL_NAME_MAXLEN		32

#define	CHANNEL_COUNT	5

#define CHN1_NAME				"ColdWhite"
#define CHN2_NAME				"Red"
#define CHN3_NAME				"Blue"
#define CHN4_NAME				"Purple"
#define CHN5_NAME				"WarmWhite"

#define	NIGHT_CHANNEL			0
#if	NIGHT_CHANNEL >= CHANNEL_COUNT
#error "NIGHT_CHANNEL must be smaller than CHANNEL_COUNT."
#endif

typedef enum {
	MANUAL,
	AUTO,
	PRO
} led_mode_t;

typedef struct {
	device_config_t super;

	unsigned last_mode : 2;									//上次的模式 Auto/Pro
	unsigned state : 2;										//Off->All->Blue->WiFi
	unsigned all_bright : 10;								//All Bright   全亮亮度
	unsigned blue_bright : 10;								//Blue Bright  夜光亮度
	unsigned reserved : 8;

	int mode;

	bool power;
	int	brights[CHANNEL_COUNT];
	int custom1Brights[CHANNEL_COUNT];
	int custom2Brights[CHANNEL_COUNT];
	int custom3Brights[CHANNEL_COUNT];
	int custom4Brights[CHANNEL_COUNT];

	int sunrise_ramp;
	int day_brights[CHANNEL_COUNT];
	int sunset_ramp;
	int night_brights[CHANNEL_COUNT];
	bool turnoff_enable;
	int turnoff_time;
} led_config_t;

typedef struct {
	const uint8_t 	channel_count;
	const char*		channel_names[CHANNEL_COUNT];

	uint32_t 		current_bright[CHANNEL_COUNT];
	uint32_t 		target_bright[CHANNEL_COUNT];
	bool 			day_rise;
	bool 			night_rise;
} led_para_t;

extern	user_device_t user_dev_led;

#endif