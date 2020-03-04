#ifndef	_ALIOT_MQTT_H
#define	_ALIOT_MQTT_H

#include "aliot_defs.h"

extern	void aliot_mqtt_connect();
extern	void aliot_mqtt_disconnect();
extern	void aliot_mqtt_init(dev_meta_info_t *dev_meta);

extern	void aliot_mqtt_report_fota_progress(const int step, const char *msg);

#endif