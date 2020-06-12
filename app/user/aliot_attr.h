#ifndef	_ALIOT_ATTR_H_
#define	_ALIOT_ATTR_H_

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "cJSON.h"
#include "aliot_mqtt.h"

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

#define	newReadAttrVtable(text_fn)			{\
												.toString = text_fn,\
											}
#define	newWriteAttrVtable(parse_fn)		{\
												.parseResult = parse_fn\
											}
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


extern	bool attrReadable(attr_t *attr);
extern	bool attrWritable(attr_t *attr);

extern	int getBoolString(attr_t *attr, char *buf);
extern	int getIntegerString(attr_t *attr, char *buf);
extern	int getTextString(attr_t *attr, char *buf);
extern	int getIntArrayString(attr_t *attr, char *buf);

extern	bool parseBool(attr_t *attr, cJSON *result);
extern	bool parseInteger(attr_t *attr, cJSON *result);
extern	bool parseText(attr_t *attr, cJSON *result);
extern	bool parseIntArray(attr_t *attr, cJSON *result);

extern	void aliot_attr_init(dev_meta_info_t *dev_meta);
extern	bool aliot_attr_assign(int idx, attr_t *attr);
extern	void aliot_attr_set_local();
extern	void aliot_attr_post(attr_t *attr);
extern	void aliot_attr_post_all();
extern	void aliot_attr_post_changed();
extern	void aliot_attr_parse_all(const char *msgid, cJSON *params, bool local);
extern	void aliot_attr_parse_get(cJSON *params, bool local);
extern	void aliot_regist_attr_set_cb(void (*callback)());

static const attr_vtable_t rdBoolVtable = newReadAttrVtable(getBoolString);
static const attr_vtable_t wrBoolVtable = newWriteAttrVtable(parseBool);
static const attr_vtable_t defBoolVtable = newAttrVtable(getBoolString, parseBool);

static const attr_vtable_t rdIntVtable = newReadAttrVtable(getIntegerString);
static const attr_vtable_t wrIntVtable = newWriteAttrVtable(parseInteger);
static const attr_vtable_t defIntVtable = newAttrVtable(getIntegerString, parseInteger);

static const attr_vtable_t rdTextVtable = newReadAttrVtable(getTextString);
static const attr_vtable_t wrTextVtable = newWriteAttrVtable(parseText);
static const attr_vtable_t defTextVtable = newAttrVtable(getTextString, parseText);

static const attr_vtable_t rdIntArrayVtable = newReadAttrVtable(getIntArrayString);
static const attr_vtable_t wrIntArrayVtable = newWriteAttrVtable(parseIntArray);
static const attr_vtable_t defIntArrayVtable = newAttrVtable(getIntArrayString, parseIntArray);

#endif