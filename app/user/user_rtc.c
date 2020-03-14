#include "user_rtc.h"
#include "osapi.h"

#define	SYNC_TIME_PERIOD			60			//同步时间周期 60秒

#define	EPOCH_YEAR					1970
#define	EPOCH_WEEKDAY				4			//1970.1.1 weekday
#define	SECONDS_PER_MINUTE			60
#define	MINUTES_PER_HOUR			60
#define	HOURS_PER_DAY				24
#define	SECONDS_PER_HOUR			(SECONDS_PER_MINUTE*MINUTES_PER_HOUR)
#define	SECONDS_PER_DAY				(SECONDS_PER_MINUTE*MINUTES_PER_HOUR*HOURS_PER_DAY)
#define	MINUTES_PER_DAY				(MINUTES_PER_HOUR*HOURS_PER_DAY)
#define	DAYS_PER_WEEK				7

static const char *TAG = "RTC";

const uint8_t days_normal_year[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const uint8_t days_leap_year[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static uint64_t current_time;
static bool synchronized;
static os_timer_t timer;
static int sync_period_cnt;

ICACHE_FLASH_ATTR static bool is_leap_year(uint16_t year) {
	if (year%4 != 0) {
		return false;
	}
	if (year%100 == 0 && year%400 != 0) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR static void user_rtc_process(void *arg) {
	date_time_t datetime;
	current_time += 1000;
	sync_period_cnt++;
	if (sync_period_cnt >= SYNC_TIME_PERIOD) {
		sync_period_cnt = 0;
		aliot_mqtt_get_sntptime();
		LOGD(TAG, "current time: %lld", current_time);
	}
}

ICACHE_FLASH_ATTR void user_rtc_set_time(const uint64_t time) {
	os_timer_disarm(&timer);

	current_time = time;
	synchronized = true;
	sync_period_cnt = 0;
	os_timer_setfn(&timer, user_rtc_process, NULL);
	os_timer_arm(&timer, 1000, 1);
}

ICACHE_FLASH_ATTR uint64_t user_rtc_get_time() {
	return current_time;
}

ICACHE_FLASH_ATTR static void user_rtc_sync_fn(void *arg) {
	aliot_mqtt_get_sntptime();
}

ICACHE_FLASH_ATTR void user_rtc_sync_time() {
	aliot_mqtt_get_sntptime();
	if (!synchronized) {
		os_timer_disarm(&timer);
		os_timer_setfn(&timer, user_rtc_sync_fn, NULL);
		os_timer_arm(&timer, 5000, 1);
	}
}

ICACHE_FLASH_ATTR bool user_rtc_is_synchronized() {
	return synchronized;
}

ICACHE_FLASH_ATTR bool user_rtc_get_datetime(date_time_t *datetime, int zone) {
	if (zone < -720 || zone > 720) {
		return false;
	}
	uint64_t time = current_time/1000 + zone*60;
	uint32_t seconds = time%SECONDS_PER_DAY;
	uint32_t days = time/SECONDS_PER_DAY;
	datetime->hour = seconds/SECONDS_PER_HOUR;
	seconds %= SECONDS_PER_HOUR;
	datetime->second = seconds%SECONDS_PER_MINUTE;
	datetime->minute = seconds/SECONDS_PER_MINUTE;
	datetime->weekday = (days+EPOCH_WEEKDAY)%DAYS_PER_WEEK;
	uint16_t year = EPOCH_YEAR;
	uint16_t month;
	uint16_t days_of_year;
	bool yleap;
	int i;
	while (1) {
		yleap = is_leap_year(year);
		if (yleap) {
			days_of_year = 366;
		} else {
			days_of_year = 365;
		}
		if (days < days_of_year) {
			datetime->year = year;
			if (yleap) {
				for (i = 0; i < sizeof(days_leap_year); i++) {
					if (days < days_leap_year[i]) {
						datetime->month = i+1;
						datetime->day = days;
						return true;
					} else {
						days -= days_leap_year[i];
					}
				}
			} else {
				for (i = 0; i < sizeof(days_leap_year); i++) {
					if (days < days_normal_year[i]) {
						datetime->month = i+1;
						datetime->day = days;
						return true;
					} else {
						days -= days_normal_year[i];
					}
				}
			}
			break;
		}
		days -= days_of_year;
		year++;
	}
	return false;
}

