#include "aliot_attr.h"
#include "udpserver.h"

#define	ATTR_COUNT_MAX				100

static cJSON* getBoolJson(aliot_attr_t *attr);
static cJSON* getIntegerJson(aliot_attr_t *attr);
static cJSON* getTextJson(aliot_attr_t *attr);
static cJSON* getIntArrayJson(aliot_attr_t *attr);

static bool parseBool(aliot_attr_t *attr, cJSON *result);
static bool parseInteger(aliot_attr_t *attr, cJSON *result);
static bool parseText(aliot_attr_t *attr, cJSON *result);
static bool parseIntArray(aliot_attr_t *attr, cJSON *result);

const attr_vtable_t rdBoolVtable = newReadAttrVtable(getBoolJson);
const attr_vtable_t wrBoolVtable = newWriteAttrVtable(parseBool);
const attr_vtable_t defBoolVtable = newAttrVtable(getBoolJson, parseBool);

const attr_vtable_t rdIntVtable = newReadAttrVtable(getIntegerJson);
const attr_vtable_t wrIntVtable = newWriteAttrVtable(parseInteger);
const attr_vtable_t defIntVtable = newAttrVtable(getIntegerJson, parseInteger);

const attr_vtable_t rdTextVtable = newReadAttrVtable(getTextJson);
const attr_vtable_t wrTextVtable = newWriteAttrVtable(parseText);
const attr_vtable_t defTextVtable = newAttrVtable(getTextJson, parseText);

const attr_vtable_t rdIntArrayVtable = newReadAttrVtable(getIntArrayJson);
const attr_vtable_t wrIntArrayVtable = newWriteAttrVtable(parseIntArray);
const attr_vtable_t defIntArrayVtable = newAttrVtable(getIntArrayJson, parseIntArray);

static aliot_attr_t *aliot_attrs[ATTR_COUNT_MAX];
static uint8_t	attr_cnt;

static aliot_attr_t* aliot_attr_get(const char *attrKey) {
	aliot_attr_t *attr;
	for (int i = 0; i < ATTR_COUNT_MAX; i++) {
		attr	= aliot_attrs[i];
		if (attr == NULL) {
			return NULL;
		}
		if (strcmp(attr->attrKey, attrKey) == 0) {
			return attr;
		}
	}
	return NULL;
}

static bool aliot_attr_readable(aliot_attr_t *attr) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->toJson == NULL) {
		return false;
	}
	return true;
}

static bool aliot_attr_writable(aliot_attr_t *attr) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->parseResult == NULL) {
		return false;
	}
	return true;
}

static cJSON* getBoolJson(aliot_attr_t *attr) {
	bool *attrValue = (bool *) attr->attrValue;
	int value = 0;
	if (*attrValue) {
		value = 1;
	}
	return cJSON_CreateNumber(value);
}

static cJSON* getIntegerJson(aliot_attr_t *attr) {
	int *attrValue = (int *) attr->attrValue;
	return cJSON_CreateNumber(*attrValue);
}

static cJSON* getTextJson(aliot_attr_t *attr) {
	char *attrValue = (char *) attr->attrValue;
	return cJSON_CreateString(attrValue);
}

static cJSON* getIntArrayJson(aliot_attr_t *attr) {
	int *attrValue = (int *) attr->attrValue;
	return cJSON_CreateIntArray(attrValue, attr->spec.size);
}

static bool parseBool(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsNumber(result) == false
		|| result->valueint < 0
		|| result->valueint > 1) {
		return false;
	}
	bool *attrValue = (bool *) attr->attrValue;
	*attrValue = result->valueint;
	return true;
}

static bool parseInteger(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsNumber(result) == false
		|| result->valueint < attr->spec.min
		|| result->valueint > attr->spec.max) {
		return false;
	}
	int *attrValue = (int *) attr->attrValue;
	*attrValue = result->valueint;
	return true;
}

static bool parseText(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsString(result) == false
		|| strlen(result->valuestring) >= attr->spec.length) {
		return false;
	}
	os_memset(attr->attrValue, 0, attr->spec.length);
	os_strcpy((char *) attr->attrValue, result->valuestring);
	return true;
}

static bool parseIntArray(aliot_attr_t *attr, cJSON *result) {
	if (cJSON_IsArray(result) == false
		|| cJSON_GetArraySize(result) != attr->spec.size) {
		return false;
	}
	uint32_t size = sizeof(int) * attr->spec.size;
	int *buf = (int *) os_zalloc(size);
	if (buf == NULL) {
		return false;
	}
	for (int i = 0; i < attr->spec.size; i++) {
		cJSON *item = cJSON_GetArrayItem(result, i);
		if (cJSON_IsNumber(item) == false) {
			os_free(buf);
			buf = NULL;
			return false;
		}
		buf[i]	= item->valueint;
	}
	os_memcpy(attr->attrValue, buf, size);
	os_free(buf);
	buf	= NULL;
	return true;
}

bool aliot_attr_add(aliot_attr_t *attr) {
	if (attr == NULL || attr_cnt >= ATTR_COUNT_MAX) {
		return false;
	}
	aliot_attrs[attr_cnt++]	= attr;
	return true;
}

uint8_t aliot_attr_count() {
	return attr_cnt;
}

cJSON* aliot_attr_get_json(bool only_changed) {
	cJSON *result = cJSON_CreateObject();
	aliot_attr_t *attr;
	for (int i = 0; i < attr_cnt; i++) {
		attr = aliot_attrs[i];
		if (aliot_attr_readable(attr) == false) {
			continue;
		}
		if (attr->changed || only_changed == false) {
			cJSON *item = attr->vtable->toJson(attr);
			if (item != NULL) {
				cJSON_AddItemToObject(result, attr->attrKey, item);
			}
			attr->changed = false;
		}
	}
	return result;
}

bool aliot_attr_parse_set(cJSON *params) {
	bool result = false;
	aliot_attr_t *attr;
	for (int i = 0; i < attr_cnt; i++) {
		attr = aliot_attrs[i];
		if (aliot_attr_writable(attr) == false) {
			continue;
		}
		cJSON *item = cJSON_GetObjectItem(params, attr->attrKey);
		if (item == NULL) {
			continue;
		}
		if (attr->vtable->parseResult(attr, item)) {
			attr->changed = true;
			result = true;
		}
	}
	return result;
}

cJSON* aliot_attr_parse_get_tojson(cJSON *params) {
	if (cJSON_IsArray(params) == false) {
		return NULL;
	}
	int size = cJSON_GetArraySize(params);
	if (size == 0) {
		return aliot_attr_get_json(false);
	}
	cJSON *result = cJSON_CreateObject();
	for (int i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(params, i);
		if (cJSON_IsString(item)) {
			aliot_attr_t *attr = aliot_attr_get(item->valuestring);
			if (aliot_attr_readable(attr) == false) {
				continue;
			}
			cJSON *cj = attr->vtable->toJson(attr);
			if (cj != NULL) {
				cJSON_AddItemToObject(result, attr->attrKey, cj);
			}
		}
	}
	return result;
}
