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
#define	FOTA_TOPIC_INFORM					"/ota/device/inform/%s/%s"	
//	sub		${productKey}	${deviceName}
#define	FOTA_TOPIC_UPGRADE					"/ota/device/upgrade/%s/%s"	
//	pub		${productKey}	${deviceName}
#define	FOTA_TOPIC_PROGRESS					"/ota/device/progress/%s/%s"	
//	pub		${productKey}	${deviceName}
#define	FOTA_TOPIC_REQUEST					"/ota/device/request/%s/%s"	

//	pub		${productKey}	${deviceName}
#define	DEVLABLE_TOPIC_UPDATE				"/sys/%s/%s/thing/deviceinfo/update"
//	sub		${productKey}	${deviceName}
#define	DEVLABLE_TOPIC_UPDATE_REPLY			"/sys/%s/%s/thing/deviceinfo/update_reply"
//	pub		${productKey}	${deviceName}
#define	DEVLABLE_TOPIC_DELETE				"/sys/%s/%s/thing/deviceinfo/delete"
//	sub		${productKey}	${deviceName}
#define	DEVLABLE_TOPIC_DELETE_REPLY			"/sys/%s/%s/thing/deviceinfo/delete_reply"

//	pub		${productKey}	${deviceName}
#define	SNTP_TOPIC_REQUEST					"/ext/ntp/%s/%s/request"
//	sub		${productKey}	${deviceName}
#define	SNTP_TOPIC_RESPONSE					"/ext/ntp/%s/%s/response"

//	pub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_PROPERTY_POST		"/sys/%s/%s/thing/event/property/post"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_PROPERTY_POST_REPLY	"/sys/%s/%s/thing/event/property/post_replay"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_PROPERTY_SET			"/sys/%s/%s/thing/service/property/set"
//	pub		${productKey}	${deviceName}	${eventName}
// #define	DEVMODEL_EVENT_TOPIC_POST			"/sys/%s/%s/thing/event/%s/post"
//	sub		${productKey}	${deviceName}	${eventName}
// #define	DEVMODEL_EVENT_TOPIC_POST_REPLY		"/sys/%s/%s/thing/event/%s/post_replay"
//	pub		${productKey}	${deviceName}	${serviceName}
#define	DEVMODEL_SERVICE_TOPIC				"/sys/%s/%s/thing/service/+"
//	sub		${productKey}	${deviceName}	${serviceName}
#define	DEVMODEL_SERVICE_TOPIC_REPLY		"/sys/%s/%s/thing/service/fota_upgrade_reply"

//	pub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_HISTORY_POST			"/sys/%s/%s/thing/event/property/history/post"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_HISTORY_POST_REPLY	"/sys/%s/%s/thing/event/property/history/post_reply"

//	sub		${productKey}	${deviceName}
#define	COTA_TOPIC_PUSH						"/sys/%s/%s/thing/config/push"
//	pub		${productKey}	${deviceName}
#define	COTA_TOPIC_GET						"/sys/%s/%s/thing/config/get"
//	sub		${productKey}	${deviceName}
#define	COTA_TOPIC_GET_REPLY				"/sys/%s/%s/thing/config/get_reply"

//	pub		${productKey}	${deviceName}
#define	CUSTOM_TOPIC_UPDATE					"/%s/%s/user/update"
//	pub		${productKey}	${deviceName}
#define	CUSTOM_TOPIC_ERROR					"/%s/%s/user/update/error"
//	sub		${productKey}	${deviceName}
#define	CUSTOM_TOPIC_GET					"/%s/%s/user/get"
//	sub		${productKey}	${deviceName}
// #define	CUSTOM_TOPIC_PROPERTY_GET			"/%s/%s/user/property/get"

//	sub		${productKey}	${deviceName}
#define	CUSTOM_TOPIC_FOTA_UPGRADE			"/%s/%s/user/fota/upgrade"
//	pub		${productKey}	${deviceName}
#define	CUSTOM_TOPIC_FOTA_PROGRESS			"/%s/%s/user/fota/progress"

//	sub		${productKey}	${deviceName}
#define	RRPC_TOPIC_REQUEST					"/sys/%s/%s/rrpc/request/+"
//	pub		${productKey}	${deviceName}	${msgid}
#define	RRPC_TOPIC_RESPONSE					"/sys/%s/%s/rrpc/response/%s"

//	sub
#define	REGISTER_TOPIC						"/ext/register"

#define	SVC_GET_PROPERTIES					"get_properties"
#define	SVC_GET_DEV_DATETIME				"get_device_datetime"
#define	SVC_FOTA_UPGRADE					"fota_upgrade"
#define	SVC_FOTA_CHECK						"fota_check"

extern	void aliot_mqtt_connect();
extern	void aliot_mqtt_disconnect();
extern	void aliot_mqtt_init(dev_meta_info_t *dev_meta);
extern	void aliot_mqtt_dynregist(dev_meta_info_t *dev_meta);

extern	bool aliot_mqtt_connect_status();

extern 	void aliot_regist_fota_upgrade_cb(char* (* callback)(const cJSON *params));
extern	void aliot_regist_fota_check_cb(bool (* callback)());
extern 	void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time));

extern	char* aliot_mqtt_getid();
extern	void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos, int retain);
extern	void aliot_mqtt_get_sntptime();
extern	void aliot_mqtt_report_version();
extern	void aliot_mqtt_report_fota_progress(const int8_t step, const char *msg);
extern	void aliot_mqtt_post_property(const char *id, const char *params);

#endif