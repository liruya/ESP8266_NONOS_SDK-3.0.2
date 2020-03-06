#include "user_indicator.h"

typedef struct {
	uint32_t period;
	uint32_t flash_cnt;
	void (* toggle)();

	uint32_t count;
	os_timer_t timer;
} indicator_t;

static const char *TAG = "Indicator";

static indicator_t *pindicator;

static void ICACHE_FLASH_ATTR user_indicator_flash_cb(void *arg) {
	if (pindicator != NULL) {
		if (pindicator->flash_cnt > 0) {
			pindicator->count++;
			if (pindicator->count > pindicator->flash_cnt*2) {
				os_timer_disarm(&pindicator->timer);
				os_free(pindicator);
				pindicator = NULL;
				return;
			}
		}
		if (pindicator->toggle != NULL) {
			pindicator->toggle();
		}
	}
}

void ICACHE_FLASH_ATTR user_indicator_start(const uint32_t period, const uint32_t count, void (*const toggle)()) {
	user_indicator_stop();
	pindicator = os_zalloc(sizeof(indicator_t));
	if (pindicator == NULL) {
		ERR(TAG, "pindicator start failed -> malloc failed");
		return;
	}
	pindicator->period = period;
	pindicator->flash_cnt = count;
	pindicator->toggle = toggle;

	os_timer_disarm(&pindicator->timer);
	os_timer_setfn(&pindicator->timer, user_indicator_flash_cb, &pindicator);
	os_timer_arm(&pindicator->timer, pindicator->period, 1);
}

void ICACHE_FLASH_ATTR user_indicator_stop() {
	if (pindicator != NULL) {
		os_timer_disarm(&pindicator->timer);
		os_free(pindicator);
		pindicator = NULL;
	}
}
