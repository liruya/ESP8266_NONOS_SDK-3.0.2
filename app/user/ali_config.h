#ifndef	_ALI_CONFIG_H_
#define	_ALI_CONFIG_H_

// typedef	enum {
// 	PUBLISH,
// 	SUBSCRIBE,
// 	PUBLISH_AND_SUBSCRIBE
// } topic_type_enum_t;

// typedef struct {
// 	topic_type_enum_t type;
// 	const char *topic_fmt;
// } topic_t;

// #define	FOTA_TOPIC_INFORM					{PUBLISH, "/ota/device/inform/%s/%s"}	
// #define	FOTA_TOPIC_UPGRADE					{SUBSCRIBE, "/ota/device/upgrade/%s/%s"}	
// #define	FOTA_TOPIC_PROGRESS					{PUBLISH, "/ota/device/progress/%s/%s"}	
// #define	FOTA_TOPIC_REQUEST					{PUBLISH, "/ota/device/request/%s/%s"}	

// #define	DEVLABLE_TOPIC_UPDATE				{PUBLISH, "/sys/%s/%s/thing/deviceinfo/update"}
// #define	DEVLABLE_TOPIC_UPDATE_REPLY			{SUBSCRIBE, "/sys/%s/%s/thing/deviceinfo/update_reply"}
// #define	DEVLABLE_TOPIC_DELETE				{PUBLISH, "/sys/%s/%s/thing/deviceinfo/delete"}
// #define	DEVLABLE_TOPIC_DELETE_REPLY			{SUBSCRIBE, "/sys/%s/%s/thing/deviceinfo/delete_reply"}

// #define	SNTP_TOPIC_REQUEST					{PUBLISH, "/ext/ntp/%s/%s/request"}
// #define	SNTP_TOPIC_RESPONSE					{SUBSCRIBE, "/ext/ntp/%s/%s/response"}

// #define	DEVMODEL_PROPERTY_TOPIC_POST		{PUBLISH, "/sys/%s/%s/thing/event/property/post"}
// #define	DEVMODEL_PROPERTY_TOPIC_POST_REPLY	{SUBSCRIBE, "/sys/%s/%s/thing/event/property/post_replay"}
// #define	DEVMODEL_PROPERTY_TOPIC_SET			{PUBLISH, "/sys/%s/%s/thing/service/property/set"}
// #define	DEVMODEL_EVENT_TOPIC_POST			{PUBLISH, "/sys/%s/%s/thing/event/${tsl.event.identifer}/post"}
// #define	DEVMODEL_EVENT_TOPIC_POST_REPLY		{SUBSCRIBE, "/sys/%s/%s/thing/event/${tsl.event.identifer}/post_replay"}
// #define	DEVMODEL_SERVICE_TOPIC				{PUBLISH, "/sys/%s/%s/thing/event/${tsl.service.identifer}"
// #define	DEVMODEL_SERVICE_TOPIC_REPLY		{SUBSCRIBE, "/sys/%s/%s/thing/event/${tsl.service.identifer}_reply"}

// #define	CUSTOM_TOPIC_UPDATE					{PUBLISH, "/%s/%s/user/update"}
// #define	CUSTOM_TOPIC_ERROR					{PUBLISH, "/%s/%s/user/update/error"}
// #define	CUSTOM_TOPIC_GET					{PUBLISH_AND_SUBSCRIBE, "/%s/%s/user/get"}


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
#define	DEVMODEL_PROPERTY_TOPIC_POST		"/sys/%s/%s/thing/event/property/post"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_PROPERTY_TOPIC_POST_REPLY	"/sys/%s/%s/thing/event/property/post_replay"
//	sub		${productKey}	${deviceName}
#define	DEVMODEL_PROPERTY_TOPIC_SET			"/sys/%s/%s/thing/service/property/set"
// #define	DEVMODEL_EVENT_TOPIC_POST			"/sys/%s/%s/thing/event/${tsl.event.identifer}/post"
// #define	DEVMODEL_EVENT_TOPIC_POST_REPLY		"/sys/%s/%s/thing/event/${tsl.event.identifer}/post_replay"
// #define	DEVMODEL_SERVICE_TOPIC				"/sys/%s/%s/thing/event/${tsl.service.identifer}"
// #define	DEVMODEL_SERVICE_TOPIC_REPLY		"/sys/%s/%s/thing/event/${tsl.service.identifer}_reply"

#define	CUSTOM_TOPIC_UPDATE					"/%s/%s/user/update"
#define	CUSTOM_TOPIC_ERROR					"/%s/%s/user/update/error"
#define	CUSTOM_TOPIC_GET					"/%s/%s/user/get"

extern 	subscribe_topic_t ALIOT_SUBSCRIBE_TOPICS[7];

#endif