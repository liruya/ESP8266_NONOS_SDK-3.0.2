#include "wrappers_product.h"
#include "wrappers_system.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

// #define	REGION					"cn-shanghai"
// #define	PRODUCT_KEY				"a1layga4ANI"
// #define	PRODUCT_SECRET			"Ac88WSgMUQsGP5Dr"

// #define	REGION					"us-east-1"
// #define	PRODUCT_KEY				"a4OMhQGUyYU"
// #define	PRODUCT_SECRET			"PU8uy75YYzRKdzy3"

#define	FIRMWARE_VERSION		1

#define	ALIOT_PARAM_SECTOR			0x1F0

#define	ALIOT_PARAM_OFFSET			0
#define	REGION_OFFSET				ALIOT_PARAM_OFFSET
#define	REGION_LEN					32
#define	PRODUCT_KEY_OFFSET			(REGION_OFFSET+REGION_LEN)
#define	PRODUCT_KEY_LEN				32
#define	PRODUCT_SECRET_OFFSET		(PRODUCT_KEY_OFFSET+PRODUCT_KEY_LEN)
#define	PRODUCT_SECRET_LEN			32
#define	DEVICE_SECRET_OFFSET		(PRODUCT_SECRET_OFFSET+PRODUCT_SECRET_LEN)
#define	DEVICE_SECRET_LEN			64

typedef struct {
	char region[REGION_LEN];
	char productKey[PRODUCT_KEY_LEN];
	char productSecret[PRODUCT_SECRET_LEN];
	char deviceSecret[DEVICE_SECRET_LEN];
} aliot_param_t;

aliot_param_t aliot_param;

ICACHE_FLASH_ATTR static bool valid_para(const char *para) {
	int i;
	for (i = 0 ; i < os_strlen(para); i++) {
		if (para[i] >= '0' && para[i] <= '9') {

		} else if (para[i] >= 'a' && para[i] <= 'z') {

		} else if (para[i] >= 'A' && para[i] <= 'Z') {

		} else if (para[i] == '-'){

		} else {
			return false;
		}
	}
	return true;
}

ICACHE_FLASH_ATTR bool hal_get_region(char *pregion) {
	// os_strcpy(pregion, REGION);
	// return true;

	char para[REGION_LEN+1];
	os_memset(para, 0, sizeof(para));
	if (system_param_load(ALIOT_PARAM_SECTOR, REGION_OFFSET, para, REGION_LEN)) {
		if (valid_para(para)) {
			os_strcpy(pregion, para);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_product_key(char *pkey) {
	// os_strcpy(pkey, PRODUCT_KEY);
	// return true;

	char para[PRODUCT_KEY_LEN+1];
	os_memset(para, 0, sizeof(para));
	if (system_param_load(ALIOT_PARAM_SECTOR, PRODUCT_KEY_OFFSET, para, PRODUCT_KEY_LEN)) {
		if (valid_para(para)) {
			os_strcpy(pkey, para);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_product_secret(char *psecret) {
	// os_strcpy(psecret, PRODUCT_SECRET);
	// return true;

	char para[PRODUCT_SECRET_LEN+1];
	os_memset(para, 0, sizeof(para));
	if (system_param_load(ALIOT_PARAM_SECTOR, PRODUCT_SECRET_OFFSET, para, PRODUCT_SECRET_LEN)) {
		if (valid_para(para)) {
			os_strcpy(psecret, para);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_device_name(char *dname) {
	uint8_t mac[6];
	if (wifi_get_macaddr(STATION_IF, mac)) {
		hal_sprintf(dname, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		dname[12] = '\0';
		return true;
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_device_secret(char *dsecret) {
	char para[DEVICE_SECRET_LEN+1];
	os_memset(para, 0, sizeof(para));
	if (system_param_load(ALIOT_PARAM_SECTOR, DEVICE_SECRET_OFFSET, para, DEVICE_SECRET_LEN)) {
		if (valid_para(para)) {
			os_strcpy(dsecret, para);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_region(const char *region) {
	if (valid_para(region) && os_strlen(region) < REGION_LEN) {
		if (system_param_load(ALIOT_PARAM_SECTOR, ALIOT_PARAM_OFFSET, &aliot_param, sizeof(aliot_param))) {
			os_memset(aliot_param.region, 0, REGION_LEN);
			os_strcpy(aliot_param.region, region);
			return system_param_save_with_protect(ALIOT_PARAM_SECTOR, &aliot_param, sizeof(aliot_param));
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_product_key(const char *pkey) {
	if (valid_para(pkey) && os_strlen(pkey) < PRODUCT_KEY_LEN) {
		if (system_param_load(ALIOT_PARAM_SECTOR, ALIOT_PARAM_OFFSET, &aliot_param, sizeof(aliot_param))) {
			os_memset(aliot_param.productKey, 0, PRODUCT_KEY_LEN);
			os_strcpy(aliot_param.productKey, pkey);
			return system_param_save_with_protect(ALIOT_PARAM_SECTOR, &aliot_param, sizeof(aliot_param));
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_product_secret(const char *psecret) {
	if (valid_para(psecret) && os_strlen(psecret) < PRODUCT_SECRET_LEN) {
		if (system_param_load(ALIOT_PARAM_SECTOR, ALIOT_PARAM_OFFSET, &aliot_param, sizeof(aliot_param))) {
			os_memset(aliot_param.productSecret, 0, PRODUCT_SECRET_LEN);
			os_strcpy(aliot_param.productSecret, psecret);
			return system_param_save_with_protect(ALIOT_PARAM_SECTOR, &aliot_param, sizeof(aliot_param));
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_device_secret(const char *dsecret) {
	if (valid_para(dsecret) && os_strlen(dsecret) < DEVICE_SECRET_LEN) {
		if (system_param_load(ALIOT_PARAM_SECTOR, ALIOT_PARAM_OFFSET, &aliot_param, sizeof(aliot_param))) {
			os_memset(aliot_param.deviceSecret, 0, DEVICE_SECRET_LEN);
			os_strcpy(aliot_param.deviceSecret, dsecret);
			return system_param_save_with_protect(ALIOT_PARAM_SECTOR, &aliot_param, sizeof(aliot_param));
		}
		// char para[DEVICE_SECRET_LEN+1];
		// os_memset(para, 0, sizeof(para));
		// os_strcpy(para, dsecret);
		// return system_param_save_with_protect(ALIOT_PARAM_SECTOR, para, sizeof(para));
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_version(uint16_t *version) {
	*version = FIRMWARE_VERSION;
	return true;
}