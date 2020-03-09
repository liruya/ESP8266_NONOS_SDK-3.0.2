#include "user_rtc.h"
#include "osapi.h"

#define	SYNC_TIME_PERIOD			60			//同步时间周期 60秒

static const char *TAG = "RTC";

static uint64_t current_time;
static bool synchronized;
static os_timer_t timer;
static int sync_period_cnt;

ICACHE_FLASH_ATTR static void user_rtc_process(void *arg) {
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
	if (synchronized) {
		aliot_mqtt_get_sntptime();
	} else {
		os_timer_disarm(&timer);
		os_timer_setfn(&timer, user_rtc_sync_fn, NULL);
		os_timer_arm(&timer, 5000, 1);
	}
}

