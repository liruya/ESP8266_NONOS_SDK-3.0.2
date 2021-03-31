#include "user_rtc.h"
#include "osapi.h"

static const char *TAG = "RTC";

const uint8_t days_normal_year[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
const uint8_t days_leap_year[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static os_timer_t timer;
volatile static uint64_t current_time;
static uint64_t	synchronized_time;

ICACHE_FLASH_ATTR bool user_rtc_leapyear(uint16_t year) {
	if (year%4 != 0) {
		return false;
	}
	if (year%100 == 0 && year%400 != 0) {
		return false;
	}
	return true;
}

ICACHE_FLASH_ATTR static void user_rtc_timer_cb(void *arg) {
	current_time += 1000;
}

ICACHE_FLASH_ATTR void user_rtc_set_time(const uint64_t time) {
	os_timer_disarm(&timer);

	current_time = time;
	os_timer_setfn(&timer, user_rtc_timer_cb, NULL);
	os_timer_arm(&timer, 1000, 1);
	synchronized_time = time;
}

ICACHE_FLASH_ATTR uint64_t user_rtc_get_time() {
	return current_time;
}

ICACHE_FLASH_ATTR uint64_t user_rtc_get_synchronized_time() {
	return synchronized_time;
}

ICACHE_FLASH_ATTR bool user_rtc_is_synchronized() {
	return synchronized_time > 0;
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
		yleap = user_rtc_leapyear(year);
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

