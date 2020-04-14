#ifndef	_USER_DEVICE_H_
#define	_USER_DEVICE_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "user_key.h"
#include "user_task.h"
#include "user_smartconfig.h"
#include "user_apconfig.h"
#include "user_indicator.h"
#include "cJSON.h"
#include "aliot_attr.h"

#define	SMARTCONFIG_TIEMOUT			60000
#define	SMARTCONFIG_FLASH_PERIOD	500
#define	APCONFIG_TIMEOUT			120000
#define	APCONFIG_FLASH_PERIOD		1500

#define	KEY_LONG_PRESS_COUNT		200			//长按键周期

#define	CONFIG_SAVED_FLAG			0x55		//设备配置保存标志
#define	CONFIG_DEFAULT_FLAG			0xFF

#define SPI_FLASH_SECTOR_SIZE       4096
/*need  to define for the User Data section*/
/* 2MBytes  512KB +  512KB  0x7C */
/* 2MBytes 1024KB + 1024KB  0xFC */
#define PRIV_PARAM_START_SECTOR     0x7D

#define TIME_VALUE_MAX				1439
#define	SUNRISE_DEFAULT				420			//07:00
#define	SUNSET_DEFAULT				1080		//18:00

typedef struct {
	char mac[16];
	char ssid[33];
	char ipaddr[16];
	int8_t	rssi;
} device_info_t;

typedef struct {
	int saved_flag;
	int local_psw;
	int zone;
	int sunrise;
	int sunset;
} device_config_t;

typedef struct {
	const char *product;				//产品类型 用于AP配网模式作为SSID前缀 长度必须小于25
	char apssid[33];					//AP配网模式SSID
	char device_time[23];				//设备时钟
	int	firmware_version;				//固件版本
	device_info_t dev_info;				//设备信息

	const uint8_t key_io_num;			//按键IO
	const uint8_t test_led1_num;		//进入产测模式指示
	const uint8_t test_led2_num;

	void (*const board_init)();
	void (*const init)();
	void (*const process)(void *);
	void (*const settime)(int, long);

	attr_t attrDeviceInfo;
	attr_t attrFirmwareVersion;
	attr_t attrZone;
	attr_t attrDeviceTime;
	attr_t attrSunrise;
	attr_t attrSunset;
} user_device_t;

extern	void user_device_attch_instance(user_device_t *pdev);
extern	void user_device_board_init();
extern	void user_device_init();
extern	int user_device_get_version();
extern	int getDeviceInfoString(attr_t *attr, char *buf);

static const attr_vtable_t deviceInfoVtable = newReadAttrVtable(getDeviceInfoString);

#endif