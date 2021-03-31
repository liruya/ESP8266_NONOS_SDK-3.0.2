#ifndef	_ALIOT_DEFS_H_
#define	_ALIOT_DEFS_H_

#include "aliot_types.h"

#define REGION                      "region"
#define PRODUCT_KEY                 "productKey"
#define PRODUCT_SECRET              "productSecret"
#define DEVICE_NAME                 "deviceName"
#define DEVICE_SECRET               "deviceSecret"
#define MAC_ADDRESS                 "mac"

#define REGION_LEN                  32
#define	PRODUCT_KEY_LEN				32
#define	PRODUCT_SECRET_LEN			32
#define	DEVICE_NAME_LEN				32
#define	DEVICE_SECRET_LEN			64

typedef struct {
    const char * const region;
    const char * const product_key;
    const char * const product_secret;
    char device_name[DEVICE_NAME_LEN + 4];
    char device_secret[DEVICE_SECRET_LEN + 4];
} dev_meta_info_t;

#endif