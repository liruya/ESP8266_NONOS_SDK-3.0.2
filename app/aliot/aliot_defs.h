#ifndef	_ALIOT_DEFS_H_
#define	_ALIOT_DEFS_H_

#include "aliot_types.h"

#define REGION_LEN                  32
#define	PRODUCT_KEY_LEN				16
#define	PRODUCT_SECRET_LEN			32
#define	DEVICE_NAME_LEN				32
#define	DEVICE_SECRET_LEN			48

typedef struct {
    char region[REGION_LEN + 1];
    char product_key[PRODUCT_KEY_LEN + 1];
    char product_secret[PRODUCT_SECRET_LEN + 1];
    char device_name[DEVICE_NAME_LEN + 1];
    char device_secret[DEVICE_SECRET_LEN + 1];
    uint16_t firmware_version;
} dev_meta_info_t;

#endif