#ifndef	_ALIOT_ATTR_H_
#define	_ALIOT_ATTR_H_

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "cJSON.h"
#include "aliot_mqtt.h"

#define	ATTR_COUNT_MAX				100

#define	KEY_FMT						"\"%s\":"
#define	KEY_NUMBER_FMT				"\"%s\":%d"
#define	KEY_TEXT_FMT				"\"%s\":\"%s\""
#define	KEY_STRUCT_FMT				"\"%s\":{%s}"
#define	KEY_ARRAY_FMT				"\"%s\":[%s]"

typedef struct _attr 	attr_t;

typedef struct {
	int (* toString)(attr_t *attr, char *text);
	bool (* parseResult)(attr_t *attr, cJSON *result);
} attr_vtable_t;

typedef union {
	//	Integer
	struct {
		int min;
		int max;
	};
	//	Text
	struct {
		int length;
	};
	//	Array
	struct {
		int size;
	};
} spec_t;

struct _attr {
	const char *attrKey;
	void * const attrValue;
	const spec_t spec;
	bool changed;
	const attr_vtable_t *vtable;
};

#define	newAttrVtable(text_fn, parse_fn)	{\
												.toString = text_fn,\
												.parseResult = parse_fn\
											}
#define	newAttr(name, value, spc, vtb)		{\
												.attrKey = name,\
												.attrValue = value,\
												.spec = spc,\
												.vtable = vtb\
											}
#define	newBoolAttr(name, value, vtb)		{\
												.attrKey = name,\
												.attrValue = value,\
												.vtable = vtb\
											}
#define	newIntAttr(name, value, l, h, vtb)	{\
												.attrKey = name,\
												.attrValue = value,\
												.spec = {\
													.min = l,\
													.max = h\
												},\
												.vtable = vtb\
											}
#define	newTextAttr(name, value, len, vtb)	{\
												.attrKey = name,\
												.attrValue = value,\
												.spec = {\
													.length = len,\
												},\
												.vtable = vtb\
											}
#define	newArrayAttr(name, value, sz, vtb)	{\
												.attrKey = name,\
												.attrValue = value,\
												.spec = {\
													.size = sz,\
												},\
												.vtable = vtb\
											}


extern	int getBoolString(attr_t *attr, char *buf);
extern	int getIntegerString(attr_t *attr, char *buf);
extern	int getTextString(attr_t *attr, char *buf);
extern	int getIntArrayString(attr_t *attr, char *buf);

extern	bool parseBool(attr_t *attr, cJSON *result);
extern	bool parseInteger(attr_t *attr, cJSON *result);
extern	bool parseText(attr_t *attr, cJSON *result);
extern	bool parseIntArray(attr_t *attr, cJSON *result);

extern	bool aliot_attr_assign(int idx, attr_t *attr);
extern	void aliot_attr_post(attr_t *attr);
extern	void aliot_attr_post_all();
extern	void aliot_attr_post_changed();
extern	void aliot_attr_parse_all(cJSON *params);
extern	void aliot_attr_parse_get(cJSON *params);
extern	void aliot_regist_attr_set_cb(void (*callback)());

static const attr_vtable_t defBoolAttrVtable = newAttrVtable(getBoolString, parseBool);
static const attr_vtable_t defIntAttrVtable = newAttrVtable(getIntegerString, parseInteger);
static const attr_vtable_t defTextAttrVtable = newAttrVtable(getTextString, parseText);
static const attr_vtable_t defIntArrayAttrVtable = newAttrVtable(getIntArrayString, parseIntArray);

#endif