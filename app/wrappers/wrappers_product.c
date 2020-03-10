#include "wrappers_product.h"
#include "wrappers_system.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

// #define	REGION					"cn-shanghai"
// #define	PRODUCT_KEY				"a1layga4ANI"
// #define	PRODUCT_SECRET			"Ac88WSgMUQsGP5Dr"

#define	REGION					"us-east-1"
#define	PRODUCT_KEY				"a4OMhQGUyYU"
#define	PRODUCT_SECRET			"PU8uy75YYzRKdzy3"

#define	FIRMWARE_VERSION		1

#define	DEVICE_SECRET_LEN		48
#define	DEVICE_SECRET_SECTOR	0x1F0

typedef struct {
	char deviceSecret[48];
} priv_para_t;

static priv_para_t para;

ICACHE_FLASH_ATTR bool hal_get_region(char *pregion) {
	os_strcpy(pregion, REGION);
	return true;
}

ICACHE_FLASH_ATTR bool hal_get_product_key(char *pkey) {
	os_strcpy(pkey, PRODUCT_KEY);
	return true;
}

ICACHE_FLASH_ATTR bool hal_get_product_secret(char *psecret) {
	os_strcpy(psecret, PRODUCT_SECRET);
	return true;
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

ICACHE_FLASH_ATTR bool valid_device_secret(const char *dsecret) {
	if (os_strlen(dsecret) > DEVICE_SECRET_LEN) {
		return false;
	}
	int i;
	for (i = 0 ; i < os_strlen(dsecret); i++) {
		if (dsecret[i] >= '0' && dsecret[i] <= '9') {

		} else if (dsecret[i] >= 'a' && dsecret[i] <= 'z') {

		} else if (dsecret[i] >= 'A' && dsecret[i] <= 'Z') {

		} else {
			return false;
		}
	}
	return true;
}

ICACHE_FLASH_ATTR bool hal_get_device_secret(char *dsecret) {
	char para[DEVICE_SECRET_LEN+1];
	os_memset(para, 0, sizeof(para));
	if (system_param_load(DEVICE_SECRET_SECTOR, 0, para, sizeof(para))) {
		if (valid_device_secret(para)) {
			os_strcpy(dsecret, para);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_device_secret(const char *dsecret) {
	if (valid_device_secret(dsecret)) {
		char para[DEVICE_SECRET_LEN+1];
		os_memset(para, 0, sizeof(para));
		os_strcpy(para, dsecret);
		return system_param_save_with_protect(DEVICE_SECRET_SECTOR, para, sizeof(para));
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_version(uint16_t *version) {
	*version = FIRMWARE_VERSION;
	return true;
}