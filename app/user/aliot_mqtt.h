#ifndef	_ALIOT_MQTT_H
#define	_ALIOT_MQTT_H

#include "aliot_defs.h"
#include "cJSON.h"

typedef struct _subscribe_topic subscribe_topic_t;

struct _subscribe_topic {
	const char *topic_fmt;
	const int qos;
	void (*parse_function)(const char *payload);
	void (*parse_ext_function)(const char *ext, const char *payload);
};

typedef struct {
	const char *key;
	const char *value;
} key_value_t;

//	pub		${productKey}	${deviceName}
#define	TOPIC_FOTA_INFORM					"/ota/device/inform/%s/%s"	
//	sub		${productKey}	${deviceName}
#define	TOPIC_FOTA_UPGRADE					"/ota/device/upgrade/%s/%s"	
//	pub		${productKey}	${deviceName}
#define	TOPIC_FOTA_PROGRESS					"/ota/device/progress/%s/%s"	
//	pub		${productKey}	${deviceName}
#define	TOPIC_FOTA_REQUEST					"/ota/device/request/%s/%s"	

//	pub		${productKey}	${deviceName}
#define	TOPIC_DEVLABLE_UPDATE				"/sys/%s/%s/thing/deviceinfo/update"
//	sub		${productKey}	${deviceName}
#define	TOPIC_DEVLABLE_UPDATE_REPLY			"/sys/%s/%s/thing/deviceinfo/update_reply"
//	pub		${productKey}	${deviceName}
#define	TOPIC_DEVLABLE_DELETE				"/sys/%s/%s/thing/deviceinfo/delete"
//	sub		${productKey}	${deviceName}
#define	TOPIC_DEVLABLE_DELETE_REPLY			"/sys/%s/%s/thing/deviceinfo/delete_reply"

//	pub		${productKey}	${deviceName}
#define	TOPIC_SNTP_REQUEST					"/ext/ntp/%s/%s/request"
//	sub		${productKey}	${deviceName}
#define	TOPIC_SNTP_RESPONSE					"/ext/ntp/%s/%s/response"

//	pub		${productKey}	${deviceName}
#define	TOPIC_PROPERTY_POST					"/sys/%s/%s/thing/event/property/post"
//	sub		${productKey}	${deviceName}
#define	TOPIC_PROPERTY_POST_REPLY			"/sys/%s/%s/thing/event/property/post_replay"
//	sub		${productKey}	${deviceName}
#define	TOPIC_PROPERTY_SET					"/sys/%s/%s/thing/service/property/set"

//	pub		${productKey}	${deviceName}	${eventName}
#define	TOPIC_EVENT_POST					"/sys/%s/%s/thing/event/%s/post"
//	sub		${productKey}	${deviceName}	${eventName}
#define	TOPIC_EVENT_POST_REPLY				"/sys/%s/%s/thing/event/%s/post_replay"

//	pub		${productKey}	${deviceName}	${serviceName}
#define	TOPIC_ASYNC_SERVICE_REQUEST			"/sys/%s/%s/thing/service/+"
//	sub		${productKey}	${deviceName}	${serviceName}
#define	TOPIC_ASYNC_SERVICE_REPLY			"/sys/%s/%s/thing/service/%s_reply"

//	sub		${productKey}	${deviceName}
#define	TOPIC_SYNC_SERVICE_REQUEST			"/sys/%s/%s/rrpc/request/+"
//	pub		${productKey}	${deviceName}	${msgid}
#define	TOPIC_SYNC_SERVICE_RESPONSE			"/sys/%s/%s/rrpc/response/%s"

//	pub		${productKey}	${deviceName}
#define	TOPIC_HISTORY_POST					"/sys/%s/%s/thing/event/property/history/post"
//	sub		${productKey}	${deviceName}
#define	TOPIC_HISTORY_POST_REPLY			"/sys/%s/%s/thing/event/property/history/post_reply"

//	sub		${productKey}	${deviceName}
#define	TOPIC_CUSTOM_ALL					"/%s/%s/user/#"

typedef enum {
	MQTT_RECV_TYPE_SNTP,
	MQTT_RECV_TYPE_PROPERTY_SET,
	MQTT_RECV_TYPE_ASYNC_SERVICE,
	MQTT_RECV_TYPE_SYNC_SERVICE,
	MQTT_RECV_TYPE_CUSTOM
} aliot_mqtt_recv_type_t;

typedef struct {
	char *msgid;
	uint32_t	code;
	char *data;
	uint32_t data_len;
	char *message;
	uint32_t message_len;
} aliot_mqtt_recv_generic_reply_t;

typedef struct {
	char *productKey;

	char *deviceName;
} aliot_mqtt_recv_t;

extern	void aliot_mqtt_connect();
extern	void aliot_mqtt_disconnect();
extern	void aliot_mqtt_init(dev_meta_info_t *dev_meta);
extern	void aliot_mqtt_deinit();

extern	bool aliot_mqtt_connect_status();

extern	void aliot_regist_mqtt_connect_cb(void (* callback)());

extern 	void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time));
extern	void aliot_regist_property_set_cb(void (*callback)(const char *msgid, cJSON *params));
extern	void aliot_regist_async_service_cb(void (*callback)(const char *msgid, const char *service_id, cJSON *params));
extern	void aliot_regist_sync_service_cb(void (*callback)(const char *rrpcid, const char *msgid, const char *service_id, cJSON *params));
extern	void aliot_regist_custom_cb(void (*callback)(const char *custom_id, cJSON *params));

extern	char* aliot_mqtt_getid();

extern	void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos);
extern	void aliot_mqtt_subscribe_topic(char *topic, const uint8_t qos);

extern	void aliot_mqtt_get_sntptime(uint64_t sendTime);
extern	void aliot_mqtt_report_version(char *version);

extern	void aliot_mqtt_post_property(const char *id, const char *params);
extern	void aliot_mqtt_async_service_reply(const char *service_id, const char *msgid, int code, const char *message);
extern	void aliot_mqtt_sync_service_reply(const char *rrpcid, const char *msgid, int code, char *data);

#endif