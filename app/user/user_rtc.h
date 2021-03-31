#ifndef	_USER_RTC_H_
#define	_USER_RTC_H_

#include "app_common.h"

#define	SYNC_RETRY_INTERVAL			10000		//	ms
#define	SYNC_TIME_PERIOD			7200		//	同步时间周期 7200秒

#define	EPOCH_YEAR					1970
#define	EPOCH_WEEKDAY				4			//1970.1.1 weekday
#define	SECONDS_PER_MINUTE			60
#define	MINUTES_PER_HOUR			60
#define	HOURS_PER_DAY				24
#define	SECONDS_PER_HOUR			(SECONDS_PER_MINUTE*MINUTES_PER_HOUR)
#define	SECONDS_PER_DAY				(SECONDS_PER_MINUTE*MINUTES_PER_HOUR*HOURS_PER_DAY)
#define	MINUTES_PER_DAY				(MINUTES_PER_HOUR*HOURS_PER_DAY)
#define	DAYS_PER_WEEK				7

typedef struct {
	uint16_t	year;
	uint8_t 	month;
	uint8_t 	day;
	uint8_t 	weekday;
	uint8_t 	hour;
	uint8_t 	minute;
	uint8_t 	second;
} date_time_t;

extern	bool user_rtc_leapyear(uint16_t year);
extern	void user_rtc_set_time(const uint64_t time);
extern	uint64_t user_rtc_get_time();
extern	uint64_t user_rtc_get_synchronized_time();
extern	bool user_rtc_is_synchronized();
extern	bool user_rtc_get_datetime(date_time_t *datetime, int zone);

#endif