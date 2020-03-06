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

#define	KEY_NUMBER_FMT				"\"%s\":%d"
#define	KEY_TEXT_FMT				"\"%s\":\"%s\""
#define	KEY_STRUCT_FMT				"\"%s\":{%s}"
#define	KEY_ARRAY_FMT				"\"%s\":[%s]"

#define	JSON_NUMBER_FMT				"{\"%s\":%d}"
#define	JSON_TEXT_FMT				"{\"%s\":\"%s\"}"
#define	JSON_STRUCT_FMT				"{\"%s\":{%s}}"
#define	JSON_ARRAY_FMT				"{\"%s\":[%s]}"			

typedef	enum {
	TYPE_INT,
	TYPE_FLOAT,
	TYPE_DOUBLE,
	TYPE_ENUM,
	TYPE_BOOL,
	TYPE_TEXT,
	TYPE_DATE,
	TYPE_STRUCT,
	TYPE_ARRAY
} data_type_t;

typedef struct _property property_t;

struct _property {
	//	属性名称
	const char *attrKey;
	//	属性值			
	void *attrValue;
	//	属性值转为字符串
	int (*getValueText)(property_t *prop, char *text);
};

typedef	struct _attr_vtable attr_vtable_t;
typedef struct _attr 		attr_t;

struct _attr_vtable {
	int (* toText)(attr_t *attr, char *text);
};

struct _attr {
	const char *attrKey;
	void * const attrValue;
	const attr_vtable_t *vtable;
};

typedef struct {
	int saved_flag;
	int local_psw;
	int zone;
	int sunrise;
	int sunset;
} device_config_t;

typedef struct {
	const uint8_t key_io_num;
	const uint8_t test_led1_num;
	const uint8_t test_led2_num;

	void (*const board_init)();
	void (*const init)();
	void (*const process)(void *);
} user_device_t;

#define	newAttrVtable(toText)		{toText}
#define	newAttrNumberVtable()		{getNumberString}
#define	newAttrTextVtable()			{getTextString}

extern	void user_device_init(user_device_t *pdev);

extern	int getNumberString(attr_t *attr, char *buf);
extern	int getTextString(attr_t *attr, char *buf);
extern	void user_device_post_property(attr_t *attr);

#endif