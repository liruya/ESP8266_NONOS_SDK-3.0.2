#ifndef	_ALIOT_MQTT_H
#define	_ALIOT_MQTT_H

#include "aliot_defs.h"

typedef struct {
	const char *topic_fmt;
	const int qos;
	void (*parse_function)(const char *payload);
} subscribe_topic_t;

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
// #define	DEVMODEL_SERVICE_TOPIC				"/sys/%s/%s/thing/event/%s"
//	sub		${productKey}	${deviceName}	${serviceName}
// #define	DEVMODEL_SERVICE_TOPIC_REPLY		"/sys/%s/%s/thing/event/%s_reply"

//	pub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_HISTORY_POST			"/sys/%s/%s/thing/event/property/history/post"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_TOPIC_HISTORY_POST_REPLY	"/sys/%s/%s/thing/event/property/history/post_reply"

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

//	sub
#define	REGISTER_TOPIC						"/ext/register"

extern	void aliot_mqtt_connect();
extern	void aliot_mqtt_disconnect();
extern	void aliot_mqtt_init(dev_meta_info_t *dev_meta);
extern	void aliot_mqtt_dynregist(dev_meta_info_t *dev_meta);

extern	bool aliot_mqtt_connect_status();

extern 	void aliot_regist_fota_upgrade_cb(void (* callback)(const char *ver, const char *url));
extern 	void aliot_regist_sntp_response_cb(void (*callback)(const uint64_t time));
extern	void aliot_regist_connect_cb(void (*callback)());

extern	uint32_t aliot_mqtt_getid();
extern	void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos, int retain);
extern	void aliot_mqtt_get_sntptime();
extern	void aliot_mqtt_report_version();
extern	void aliot_mqtt_report_fota_progress(const int step, const char *msg);
extern	void aliot_mqtt_post_property(const int id, const char *params);

#endif