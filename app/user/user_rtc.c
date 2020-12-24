#include "user_rtc.h"
#include "osapi.h"

static const char *TAG = "RTC";

const uint8_t days_normal_year[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const uint8_t days_leap_year[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static os_timer_t timer;
volatile static uint64_t current_time;
volatile static bool synchronized;
volatile static uint32_t sync_period_cnt;

ICACHE_FLASH_ATTR static bool is_leap_year(uint16_t year) {
	if (year%4 != 0) {
		return false;
	}
	if (year%100 == 0 && year%400 != 0) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR void user_rtc_clear() {
	sync_period_cnt = 0;
}

ICACHE_FLASH_ATTR static void user_rtc_process(void *arg) {
	current_time += 1000;
	sync_period_cnt++;
	if (sync_period_cnt >= SYNC_TIME_PERIOD) {
		sync_period_cnt = 0;
		aliot_mqtt_get_sntptime();
		// LOGD(TAG, "current time: %lld", current_time);
	}

	// date_time_t datetime;
	// bool result = user_rtc_get_datetime(&datetime, 480);
	// if (result == false) {
	// 	return;
	// }
	// LOGD(TAG, "%04d-%02d-%02d %d %02d:%02d:%02d.%03d",
	// 	datetime.year, datetime.month, datetime.day, datetime.weekday,
	// 	datetime.hour, datetime.minute, datetime.second, current_time%1000);
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

ICACHE_FLASH_ATTR uint32_t user_rtc_get_days() {
	return current_time/(1000*SECONDS_PER_DAY);
}

ICACHE_FLASH_ATTR uint32_t user_rtc_get_seconds() {
	return (current_time/1000)%SECONDS_PER_DAY;
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
						datetime->day = days+1;
						return true;
					} else {
						days -= days_leap_year[i];
					}
				}
			} else {
				for (i = 0; i < sizeof(days_leap_year); i++) {
					if (days < days_normal_year[i]) {
						datetime->month = i+1;
						datetime->day = days+1;
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

