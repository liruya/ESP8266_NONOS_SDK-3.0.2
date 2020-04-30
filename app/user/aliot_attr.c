#include "aliot_attr.h"
#include "udpserver.h"

typedef struct {
	void (*attr_set_cb)();
} attr_callback_t;

static const char *TAG = "Attr";

static attr_t *attrs[ATTR_COUNT_MAX];
static bool from_local;

attr_callback_t attr_callback;

ICACHE_FLASH_ATTR void aliot_attr_set_local() {
	from_local = true;
}

//  ${msgid}    ${params}
#define PROPERTY_LOCAL_POST_PAYLOAD_FMT   "{\"params\":{%s}}"
/**
 * @param params: 属性转换后的json格式字符串
 * */
ICACHE_FLASH_ATTR static void aliot_attr_post_local(const char *params) {
	int len = os_strlen(PROPERTY_LOCAL_POST_PAYLOAD_FMT) + os_strlen(params) + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        os_printf("malloc payload failed.\n");
        return;
    }
    os_snprintf(payload, len, PROPERTY_LOCAL_POST_PAYLOAD_FMT, params);
    udpserver_send(payload, os_strlen(payload));
    os_free(payload);
    payload = NULL;
}

ICACHE_FLASH_ATTR static attr_t* aliot_attr_get(const char *attrKey) {
	int i;
	for (i = 0; i < ATTR_COUNT_MAX; i++) {
		if (attrs[i] != NULL && os_strcmp(attrs[i]->attrKey, attrKey) == 0) {
			return attrs[i];
		}
	}
	return NULL;
}

ICACHE_FLASH_ATTR bool attrReadable(attr_t *attr) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->toString == NULL) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR bool attrWritable(attr_t *attr) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->parseResult == NULL) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR int getBoolString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	return os_sprintf(buf, KEY_NUMBER_FMT, attr->attrKey, *((bool *) attr->attrValue));
}

ICACHE_FLASH_ATTR int getIntegerString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	return os_sprintf(buf, KEY_NUMBER_FMT, attr->attrKey, *((int *) attr->attrValue));
}

ICACHE_FLASH_ATTR int getTextString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	return os_sprintf(buf, KEY_TEXT_FMT, attr->attrKey, (char *) attr->attrValue);
}

ICACHE_FLASH_ATTR int getIntArrayString(attr_t *attr, char *buf) {
	if (attrReadable(attr) == false) {
		return 0;
	}
	if (attr->spec.size <= 0) {
		return os_sprintf(buf, "\"%s\":[]", attr->attrKey);
	}
	int *array = (int *) attr->attrValue;
	int i;
	os_sprintf(buf, KEY_FMT, attr->attrKey);
	os_sprintf(buf+os_strlen(buf), "%c", '[');
	for (i = 0; i < attr->spec.size; i++) {
		os_sprintf(buf + os_strlen(buf), "%d,", array[i]);
	}
	int len = os_strlen(buf);
	buf[len-1] = ']';
	return len;
}

ICACHE_FLASH_ATTR bool parseBool(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsNumber(result) == false) {
		return false;
	}
	if (result->valueint < 0 || result->valueint > 1) {
		return false;
	}
	*((bool *) attr->attrValue) = result->valueint;
	return true;
}

ICACHE_FLASH_ATTR bool parseInteger(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsNumber(result) == false) {
		return false;
	}
	if (result->valueint < attr->spec.min || result->valueint > attr->spec.max) {
		return false;
	}
	*((int *) attr->attrValue) = result->valueint;
	return true;
}

ICACHE_FLASH_ATTR bool parseText(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsString(result) == false) {
		return false;
	}
	if (os_strlen(result->valuestring) >= attr->spec.length) {
		return false;
	}
	os_memset(attr->attrValue, 0, attr->spec.length);
	os_strcpy((char *) attr->attrValue, result->valuestring);
	return true;
}

ICACHE_FLASH_ATTR bool parseIntArray(attr_t *attr, cJSON *result) {
	if (attrWritable(attr) == false) {
		return false;
	}
	if (cJSON_IsArray(result) == false) {
		return false;
	}
	if (cJSON_GetArraySize(result) != attr->spec.size) {
		return false;
	}
	int *buf = os_zalloc(attr->spec.size*sizeof(int));
	if (buf == NULL) {
		return false;
	}
	int *array = (int *) attr->attrValue;
	int i;
	for (i = 0; i < attr->spec.size; i++) {
		cJSON *item = cJSON_GetArrayItem(result, i);
		if (cJSON_IsNumber(item) == false) {
			os_free(buf);
			buf = NULL;
			return false;
		}
		buf[i] = item->valueint;
	}
	os_memcpy(array, buf, attr->spec.size*sizeof(int));
	os_free(buf);
	buf = NULL;
	return true;
}

ICACHE_FLASH_ATTR void aliot_attr_post(attr_t *attr) {
	if (aliot_mqtt_connect_status() == false) {
		return;
	}
	if (attrReadable(attr) == false) {
		return;
	}
	char params[1024];
	os_memset(params, 0, sizeof(params));
	attr->vtable->toString(attr, params);
	if (from_local) {
		from_local = false;
		aliot_attr_post_local(params);
	}
	aliot_mqtt_post_property(params);
	attr->changed = false;
}

/**
 * @param only_changed false-all true-only changed
 * */
ICACHE_FLASH_ATTR static void aliot_post_properties(bool only_changed) {
	if (aliot_mqtt_connect_status() == false) {
		return;
	}
	char *params = os_zalloc(10240);
	if (params == NULL) {
		return;
	}
	int i;
	for (i = 0; i < ATTR_COUNT_MAX; i++) {
		if (attrReadable(attrs[i]) == false) {
			continue;
		}
		if (attrs[i]->changed || only_changed == false) {
			int len = attrs[i]->vtable->toString(attrs[i], params + os_strlen(params));
			if (len > 0) {
				os_sprintf(params+os_strlen(params), "%c", ',');
			}
			attrs[i]->changed = false;
		}
	}
	int len = os_strlen(params);
	if (params[len-1] == ',') {
		params[len-1] = '\0';
	}
	if (from_local) {
		from_local = false;
		aliot_attr_post_local(params);
	}
	aliot_mqtt_post_property(params);
	os_free(params);
	params = NULL;
}

ICACHE_FLASH_ATTR void aliot_attr_post_all() {
	if (aliot_mqtt_connect_status() == false) {
		return;
	}
	aliot_post_properties(false);
}

ICACHE_FLASH_ATTR void aliot_attr_post_changed() {
	if (aliot_mqtt_connect_status() == false) {
		return;
	}
	aliot_post_properties(true);
}

ICACHE_FLASH_ATTR bool aliot_attr_assign(int idx, attr_t *attr) {
	if (idx < 0 || idx >= ATTR_COUNT_MAX) {
		return false;
	}
	if (attrs[idx] != NULL) {
		return false;
	}
	attrs[idx] = attr;
	return true;
}

ICACHE_FLASH_ATTR void aliot_attr_parse_all(cJSON *params) {
	bool result = false;
	int i;
	for (i = 0; i < ATTR_COUNT_MAX; i++) {
		if (attrWritable(attrs[i]) == false) {
			continue;
		}
		cJSON *item = cJSON_GetObjectItem(params, attrs[i]->attrKey);
		if (attrs[i]->vtable->parseResult(attrs[i], item)) {
			attrs[i]->changed = true;
			result = true;
		}
	}
	if (result && attr_callback.attr_set_cb != NULL) {
		attr_callback.attr_set_cb();
	}
}

ICACHE_FLASH_ATTR void aliot_attr_parse_get(cJSON *params) {
	bool result = false;
	int i;
	if (cJSON_IsArray(params)) {
		if (cJSON_GetArraySize(params) == 0) {
			aliot_attr_post_all();
		} else {
			for (i = 0; i < cJSON_GetArraySize(params); i++) {
				cJSON *item = cJSON_GetArrayItem(params, i);
				if (cJSON_IsString(item)) {
					attr_t *attr = aliot_attr_get(item->valuestring);
					if (attr != NULL) {
						attr->changed = true;
					}
				}
			}
			aliot_attr_post_changed();
		}
	}
}

ICACHE_FLASH_ATTR void aliot_regist_attr_set_cb(void (*callback)()) {
	attr_callback.attr_set_cb = callback;
}
