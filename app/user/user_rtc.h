#ifndef	_USER_RTC_H_
#define	_USER_RTC_H_

#include "aliot_mqtt.h"
#include "app_common.h"

extern	void user_rtc_set_time(const uint64_t time);
extern	uint64_t user_rtc_get_time();
extern	void user_rtc_sync_time();

#endif