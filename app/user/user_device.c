#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"

#define	KEY_VALUE_FMT	"\"%s\":%s"

ICACHE_FLASH_ATTR const char* get_payload_format(data_type_t type) {
	switch (type) {
		case TYPE_INT:
		case TYPE_ENUM:
		case TYPE_BOOL:
			return NUMBER_FORMAT;
		case TYPE_FLOAT:
		case TYPE_DOUBLE:
			return STRING_FORMAT;
		case TYPE_TEXT:
		case TYPE_DATE:
			return STRING_FORMAT;
		case TYPE_STRUCT:
			return STRUCT_FORMAT;
		case TYPE_ARRAY:
			return ARRAY_FORMAT;
		default:
			break;
	}
	return STRING_FORMAT;
}

ICACHE_FLASH_ATTR int get_number_text(property_t *prop, char *text) {
	int value = *((int *) prop->attrValue);
	int len = os_strlen(NUMBER_FORMAT) + os_strlen(prop->attrKey) + 12;
	os_memset(text, 0, len);
	os_snprintf(text, len, NUMBER_FORMAT, prop->attrKey, value);
	return os_strlen(text);
}

ICACHE_FLASH_ATTR int get_string_text(property_t *prop, char *text) {
	char *value = (char *) prop->attrValue;
	int len = os_strlen(NUMBER_FORMAT) + os_strlen(value) + 1;
	os_memset(text, 0, len);
	os_snprintf(text, len, STRING_FORMAT, prop->attrKey, value);
	return os_strlen(text);
}

// void ICACHE_FLASH_ATTR user_device_post_property(property_t *prop) {
// 	const char *payload_fmt = get_payload_format(prop->dataType);
// 	int len = os_strlen(payload_fmt) + 
// 	char *payload = os_zalloc();
// }

// void ICACHE_FLASH_ATTR user_device_post_properties(property_t *prop, int cnt) {

// }