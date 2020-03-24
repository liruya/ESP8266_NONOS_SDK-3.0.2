#ifndef	_WRAPPERS_PRODUCT_H_
#define	_WRAPPERS_PRODUCT_H_

#include "osapi.h"

// #define	PRODUCT_KEY_LEN				16
// #define	PRODUCT_SECRET_LEN			32
// #define	DEVICE_NAME_LEN				32
// #define	DEVICE_SECRET_LEN			48

// extern	int hal_get_product_key(char pkey[PRODUCT_KEY_LEN+1]);
// extern	int hal_get_product_secret(char psecret[PRODUCT_SECRET_LEN+1]);
// extern	int hal_get_device_name(char dname[DEVICE_NAME_LEN+1]);
// extern	int hal_get_device_secret(char dsecret[DEVICE_SECRET_LEN+1]);

extern	char *hal_product_get(const char *key);

extern	bool hal_get_version(uint16_t *version);

extern	bool hal_get_region(char *pregion);
extern	bool hal_get_product_key(char *pkey);
extern	bool hal_get_product_secret(char *psecret);
extern	bool hal_get_device_name(char *dname);
extern	bool hal_get_device_secret(char *dsecret);

extern	bool hal_set_region(const char *region);
extern	bool hal_set_product_key(const char *pkey);
extern	bool hal_set_product_secret(const char *psecret);
extern	bool hal_set_device_secret(const char *dsecret);

#endif