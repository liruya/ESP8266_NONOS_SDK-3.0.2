#ifndef	_USER_RTC_H_
#define	_USER_RTC_H_

#include "aliot_mqtt.h"
#include "app_common.h"

typedef struct {
	uint16_t	year;
	uint8_t 	month;
	uint8_t 	day;
	uint8_t 	weekday;
	uint8_t 	hour;
	uint8_t 	minute;
	uint8_t 	second;
} date_time_t;

extern	void user_rtc_set_time(const uint64_t time);
extern	uint64_t user_rtc_get_time();
// extern	void user_rtc_sync_time();
extern	bool user_rtc_is_synchronized();
extern	bool user_rtc_get_datetime(date_time_t *datetime, int zone);

#endif