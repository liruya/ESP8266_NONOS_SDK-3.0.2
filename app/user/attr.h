#ifndef	_ATTR_H_
#define	_ATTR_H_

typedef	struct _attr_vtable attr_vtable_t;
typedef struct _attr 		attr_t;

struct _attr_vtable {
	int (* toText)(attr_t *attr, char * const text);
};

struct _attr {
	const attr_vtable_t *vtable;
	const char *attrKey;
	void * const attrValue;
};

#endif