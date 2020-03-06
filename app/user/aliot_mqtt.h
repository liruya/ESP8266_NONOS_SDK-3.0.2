#ifndef	_ALIOT_MQTT_H
#define	_ALIOT_MQTT_H

#include "aliot_defs.h"

extern	void aliot_mqtt_connect();
extern	void aliot_mqtt_disconnect();
extern	void aliot_mqtt_init(dev_meta_info_t *dev_meta);

extern	bool aliot_mqtt_connect_status();

extern	uint32_t aliot_mqtt_getid();
extern	void aliot_mqtt_publish(const char *topic_fmt, const char *payload, int qos, int retain);
extern	void aliot_mqtt_report_fota_progress(const int step, const char *msg);
extern	void aliot_mqtt_post_powerswitch(bool power);

#endif