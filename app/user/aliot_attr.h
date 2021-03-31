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

typedef struct _attr 	aliot_attr_t;

typedef struct {
	cJSON*	(* toJson)(aliot_attr_t *attr);
	// int (* toString)(aliot_attr_t *attr, char *text);
	bool (* parseResult)(aliot_attr_t *attr, cJSON *result);
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
	const char 			*attrKey;
	void * const 		attrValue;
	const 				spec_t spec;
	bool 				changed;
	const attr_vtable_t *vtable;
};

#define	newReadAttrVtable(text_fn)			{\
												.toJson = text_fn,\
											}
#define	newWriteAttrVtable(parse_fn)		{\
												.parseResult = parse_fn\
											}
#define	newAttrVtable(text_fn, parse_fn)	{\
												.toJson = text_fn,\
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

extern	bool aliot_attr_add(aliot_attr_t * const attr);

/**
 * @brief 
 * 
 * @param[in] 		only_changed 	convert all attrs or only the changed attrs
 * @param[inout] 	payload 		the buffer to convert attrs, confirm the buffer size is enough by yourself
 * @return uint32_t length of attrs to string
 */
extern	cJSON* aliot_attr_get_json(bool only_changed);

/**
 * @brief 
 * 
 * @param params 	attrs params json format
 * @return true 	at least one attr have been parsed
 * @return false 	no attr was parsed
 */
extern	bool aliot_attr_parse_set(cJSON *params);


extern	cJSON* aliot_attr_parse_get_tojson(cJSON *params);


/* read only bool attr vtable */
extern	const attr_vtable_t rdBoolVtable;
/* write only bool attr vtable */
extern	const attr_vtable_t wrBoolVtable;
/* read & write bool attr vtable */
extern	const attr_vtable_t defBoolVtable;

/* read only integer attr vtable */
extern	const attr_vtable_t rdIntVtable;
/* write only integer attr vtable */
extern	const attr_vtable_t wrIntVtable;
/* read & write integer attr vtable */
extern	const attr_vtable_t defIntVtable;

/* read only text attr vtable */
extern	const attr_vtable_t rdTextVtable;
/* write only text attr vtable */
extern	const attr_vtable_t wrTextVtable;
/* read & write text attr vtable */
extern	const attr_vtable_t defTextVtable;

/* read only integer array attr vtable */
extern	const attr_vtable_t rdIntArrayVtable;
/* write only integer arry attr vtable */
extern	const attr_vtable_t wrIntArrayVtable;
/* read & write integer arry attr vtable */
extern	const attr_vtable_t defIntArrayVtable;

#endif