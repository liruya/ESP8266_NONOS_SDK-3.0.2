#ifndef	_USER_DEVICE_H_
#define	_USER_DEVICE_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"
#include "user_key.h"
#include "user_smartconfig.h"
#include "user_apconfig.h"
#include "user_indicator.h"
#include "cJSON.h"
#include "aliot_attr.h"


#define	PRODUCT_LED					0
#define	PRODUCT_SOCKET				1
#define	PRODUCT_MONSOON				2

#ifndef	FIRMWARE_VERSION
#define	FIRMWARE_VERSION			1
#endif

#define	MAC_ADDRESS_LEN				16
#define	APSSID_LEN_MAX				32

#define	SMARTCONFIG_TIEMOUT			60000
#define	SMARTCONFIG_FLASH_PERIOD	500
#define	APCONFIG_TIMEOUT			60000
#define	APCONFIG_FLASH_PERIOD		1500

#define	KEY_LONG_PRESS_COUNT		200			//长按键周期

#define	CONFIG_SAVED_FLAG			0x55		//设备配置保存标志
#define	CONFIG_DEFAULT_FLAG			0xFF

#define SPI_FLASH_SECTOR_SIZE       4096
/*need  to define for the User Data section*/
/* 2MBytes  512KB +  512KB  0x7C */
/* 2MBytes 1024KB + 1024KB  0xFC */
// #define PRIV_PARAM_START_SECTOR     0x7D
#define PRIV_PARAM_START_SECTOR     0x1F5

/* aliot config sector */
#define	ALIOT_CONFIG_SECTOR			0x1F0
#define	ALIOT_CONFIG_OFFSET			0x00
#define	DEVICE_SECRET_OFFSET		0x00

#define TIME_VALUE_MAX				1439
#define	SUNRISE_DEFAULT				420			//07:00
#define	SUNSET_DEFAULT				1080		//18:00

typedef struct {
	char mac[18];
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
	dev_meta_info_t meta;
	const char *product;				//产品类型 用于AP配网模式作为SSID前缀 长度必须小于25
	char mac[MAC_ADDRESS_LEN];			//mac地址 XXXXXXXXXX
	char apssid[APSSID_LEN_MAX+4];		//AP配网模式SSID
	device_info_t dev_info;				//设备信息
	char device_time[24];
	uint16_t firmware_version;

	const uint8_t key_io_num;			//按键IO
	const uint8_t test_led1_num;		//进入产测模式指示
	const uint8_t test_led2_num;

	void (*const board_init)();
	void (*const init)();
	void (*const process)(void *);
	void (*const attr_set_cb)();

	aliot_attr_t attrDeviceInfo;
	aliot_attr_t attrFirmwareVersion;
	aliot_attr_t attrZone;
	aliot_attr_t attrDeviceTime;
	aliot_attr_t attrSunrise;
	aliot_attr_t attrSunset;
} user_device_t;

extern	void 	user_device_init();
extern	char *	user_device_get(const char *key);
extern	int  	user_device_get_version();
extern	bool 	user_device_set_device_secret(const char *device_secret);
extern	int  	getDeviceInfoString(aliot_attr_t *attr, char *buf);
extern	bool 	user_device_is_secret_valid();

extern	void	user_device_post_changed();

extern	const	attr_vtable_t deviceInfoVtable;

#endif