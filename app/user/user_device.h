#ifndef	_USER_DEVICE_H_
#define	_USER_DEVICE_H_

#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "user_interface.h"

#define	NUMBER_FORMAT	"\"%s\":%d"
#define	STRING_FORMAT	"\"%s\":\"%s\""
#define	STRUCT_FORMAT	"\"%s\":{%s}"
#define	ARRAY_FORMAT	"\"%s\":[%s]"

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


#endif