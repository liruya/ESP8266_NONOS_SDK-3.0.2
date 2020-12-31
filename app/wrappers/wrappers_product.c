#include "wrappers_product.h"
#include "wrappers_system.h"
#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "aliot_defs.h"

#define	ALIOT_CONFIG_SECTOR			0x1F0

#define	ALIOT_CONFIG_OFFSET			0
#define	REGION_OFFSET				ALIOT_CONFIG_OFFSET
#define	REGION_LEN					32
#define	PRODUCT_KEY_OFFSET			(REGION_OFFSET+REGION_LEN)
#define	PRODUCT_KEY_LEN				32
#define	PRODUCT_SECRET_OFFSET		(PRODUCT_KEY_OFFSET+PRODUCT_KEY_LEN)
#define	PRODUCT_SECRET_LEN			32
#define	DEVICE_SECRET_OFFSET		(PRODUCT_SECRET_OFFSET+PRODUCT_SECRET_LEN)
#define	DEVICE_SECRET_LEN			64

#define	MAC_ADDRESS_LEN				16

typedef struct {
	char region[REGION_LEN];
	char productKey[PRODUCT_KEY_LEN];
	char productSecret[PRODUCT_SECRET_LEN];
	char deviceSecret[DEVICE_SECRET_LEN];
} aliot_config_t;

typedef struct {
	char region[REGION_LEN];
	char productKey[PRODUCT_KEY_LEN];
	char productSecret[PRODUCT_SECRET_LEN];
	char deviceSecret[DEVICE_SECRET_LEN];
	char deviceName[DEVICE_NAME_LEN];
	char macAddress[MAC_ADDRESS_LEN];
} aliot_param_t;

typedef struct {
	const char *key;
	char * const value;
} key_text_t;

aliot_config_t aliot_config;
aliot_param_t aliot_param;
int isInitialized;
const key_text_t key_texts[6] = {
	{
		.key = REGION,
		.value = aliot_param.region
	},
	{
		.key = PRODUCT_KEY,
		.value = aliot_param.productKey
	},
	{
		.key = PRODUCT_SECRET,
		.value = aliot_param.productSecret
	},
	{
		.key = DEVICE_NAME,
		.value = aliot_param.deviceName
	},
	{
		.key = DEVICE_SECRET,
		.value = aliot_param.deviceSecret
	},
	{
		.key = MAC_ADDRESS,
		.value = aliot_param.macAddress
	}
};

ICACHE_FLASH_ATTR static bool valid_para(const char *para) {
	int i;
	if (os_strlen(para) == 0) {
		return false;
	}
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

ICACHE_FLASH_ATTR static bool hal_load_param() {
	if (!isInitialized) {
		uint8_t mac[6];
		if (system_param_load(ALIOT_CONFIG_SECTOR, ALIOT_CONFIG_OFFSET, &aliot_config, sizeof(aliot_config))
			&& wifi_get_macaddr(STATION_IF, mac)) {
			os_memset(&aliot_param, 0, sizeof(aliot_param));
			if (valid_para(aliot_config.region)) {
				os_strcpy(aliot_param.region, aliot_config.region);
			}
			if (valid_para(aliot_config.productKey)) {
				os_strcpy(aliot_param.productKey, aliot_config.productKey);
			}
			if (valid_para(aliot_config.productSecret)) {
				os_strcpy(aliot_param.productSecret, aliot_config.productSecret);
			}
			if (valid_para(aliot_config.deviceSecret)) {
				os_strcpy(aliot_param.deviceSecret, aliot_config.deviceSecret);
			}
			hal_sprintf(aliot_param.deviceName, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			hal_sprintf(aliot_param.macAddress, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			isInitialized = true;
		}
	}
	return isInitialized;
}

ICACHE_FLASH_ATTR char *hal_product_get(const char *key) {
	if (key == NULL) {
		return NULL;
	}
	int i;
	for (i = 0; i < sizeof(key_texts)/sizeof(key_text_t); i++) {
		if (os_strcmp(key_texts[i].key, key) == 0) {
			return key_texts[i].value;
		}
	}
	return NULL;
}

ICACHE_FLASH_ATTR bool hal_get_region(char *pregion) {
	if (hal_load_param()) {
		if (valid_para(aliot_param.region)) {
			os_strcpy(pregion, aliot_param.region);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_product_key(char *pkey) {
	if (hal_load_param()) {
		if (valid_para(aliot_param.productKey)) {
			os_strcpy(pkey, aliot_param.productKey);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_product_secret(char *psecret) {
	if (hal_load_param()) {
		if (valid_para(aliot_param.productSecret)) {
			os_strcpy(psecret, aliot_param.productSecret);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_device_name(char *dname) {
	if (hal_load_param()) {
		if (valid_para(aliot_param.deviceName)) {
			os_strcpy(dname, aliot_param.deviceName);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_get_device_secret(char *dsecret) {
	if (hal_load_param()) {
		if (valid_para(aliot_param.deviceSecret)) {
			os_strcpy(dsecret, aliot_param.deviceSecret);
			return true;
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_region(const char *region) {
	if (valid_para(region) && os_strlen(region) < REGION_LEN && os_strlen(region) > 0) {
		if (system_param_load(ALIOT_CONFIG_SECTOR, ALIOT_CONFIG_OFFSET, &aliot_config, sizeof(aliot_config))) {
			os_memset(aliot_config.region, 0, REGION_LEN);
			os_strcpy(aliot_config.region, region);
			if (system_param_save_with_protect(ALIOT_CONFIG_SECTOR, &aliot_config, sizeof(aliot_config))) {
				os_memset(aliot_param.region, 0, REGION_LEN);
				os_strcpy(aliot_param.region, region);
				return true;
			}
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_product_key(const char *pkey) {
	if (valid_para(pkey) && os_strlen(pkey) < PRODUCT_KEY_LEN && os_strlen(pkey) > 0) {
		if (system_param_load(ALIOT_CONFIG_SECTOR, ALIOT_CONFIG_OFFSET, &aliot_config, sizeof(aliot_config))) {
			os_memset(aliot_config.productKey, 0, PRODUCT_KEY_LEN);
			os_strcpy(aliot_config.productKey, pkey);
			if (system_param_save_with_protect(ALIOT_CONFIG_SECTOR, &aliot_config, sizeof(aliot_config))) {
				os_memset(aliot_param.productKey, 0, PRODUCT_KEY_LEN);
				os_strcpy(aliot_param.productKey, pkey);
				return true;
			}
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_product_secret(const char *psecret) {
	if (valid_para(psecret) && os_strlen(psecret) < PRODUCT_SECRET_LEN && os_strlen(psecret) > 0) {
		if (system_param_load(ALIOT_CONFIG_SECTOR, ALIOT_CONFIG_OFFSET, &aliot_config, sizeof(aliot_config))) {
			os_memset(aliot_config.productSecret, 0, PRODUCT_SECRET_LEN);
			os_strcpy(aliot_config.productSecret, psecret);
			if (system_param_save_with_protect(ALIOT_CONFIG_SECTOR, &aliot_config, sizeof(aliot_config))) {
				os_memset(aliot_param.productSecret, 0, PRODUCT_SECRET_LEN);
				os_strcpy(aliot_param.productSecret, psecret);
				return true;
			}
		}
	}
	return false;
}

ICACHE_FLASH_ATTR bool hal_set_device_secret(const char *dsecret) {
	if (valid_para(dsecret) && os_strlen(dsecret) < DEVICE_SECRET_LEN && os_strlen(dsecret) > 0) {
		if (system_param_load(ALIOT_CONFIG_SECTOR, ALIOT_CONFIG_OFFSET, &aliot_config, sizeof(aliot_config))) {
			os_memset(aliot_config.deviceSecret, 0, DEVICE_SECRET_LEN);
			os_strcpy(aliot_config.deviceSecret, dsecret);
			if (system_param_save_with_protect(ALIOT_CONFIG_SECTOR, &aliot_config, sizeof(aliot_config))) {
				os_memset(aliot_param.deviceSecret, 0, DEVICE_SECRET_LEN);
				os_strcpy(aliot_param.deviceSecret, dsecret);
				return true;
			}
		}
	}
	return false;
}
