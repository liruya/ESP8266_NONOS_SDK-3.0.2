#include "user_smartconfig.h"

typedef struct {
	task_t super;
	os_timer_t timer;
} smartconfig_task_t;

static bool user_smartconfig_start(task_t **task);
static bool user_smartconfig_stop(task_t **task);
static void user_smartconfig_timeout_cb();

static smartconfig_task_t *sc_task;
static const task_vtable_t sc_vtable = newTaskVTable(user_smartconfig_start, user_smartconfig_stop, user_smartconfig_timeout_cb);

static const char *TAG = "SmartConfig";

ESPFUNC static void user_smartconfig_done(sc_status status, void *pdata) {
	switch (status) {
		case SC_STATUS_WAIT:
			LOGD(TAG, "SC_STATUS_WAIT");
			break;
		case SC_STATUS_FIND_CHANNEL:
			LOGD(TAG, "SC_STATUS_FIND_CHANNEL");
			break;
		case SC_STATUS_GETTING_SSID_PSWD:
			LOGD(TAG, "SC_STATUS_GETTING_SSID_PSWD");
			sc_type *type = (sc_type *) pdata;
			if (*type == SC_TYPE_ESPTOUCH) {
				LOGD(TAG, "SC_TYPE:SC_TYPE_ESPTOUCH");
			} else {
				LOGD(TAG, "SC_TYPE:SC_TYPE_AIRKISS");
			}
			break;
		case SC_STATUS_LINK:
			LOGD(TAG, "SC_STATUS_LINK");
			wifi_station_disconnect();
			struct station_config *sta_conf =  pdata;
			wifi_station_set_config(sta_conf);
			wifi_station_connect();
			break;
		case SC_STATUS_LINK_OVER:
			LOGD(TAG, "SC_STATUS_LINK_OVER");
			if (pdata != NULL) {
				//SC_TYPE_ESPTOUCH
				uint8 phone_ip[4] = { 0 };
				os_memcpy(phone_ip, (uint8*) pdata, 4);
				LOGD(TAG, "Phone ip: %d.%d.%d.%d", phone_ip[0], phone_ip[1],
						phone_ip[2], phone_ip[3]);
			} else {
				//SC_TYPE_AIRKISS - support airkiss v2.0
			}
			user_task_stop((task_t **) &sc_task);
			break;
	}
}

ESPFUNC static bool user_smartconfig_start(task_t **task) {
	LOGD(TAG, "smartconfig start...");
	if (task == NULL || *task == NULL) {
		return false;
	}
	smartconfig_task_t *ptask = (smartconfig_task_t *) (*task);
	wifi_station_disconnect();
	smartconfig_stop();
	wifi_set_opmode(STATION_MODE);
	smartconfig_set_type(SC_TYPE_ESPTOUCH);
	smartconfig_start(user_smartconfig_done);
	return true;
}

ESPFUNC static void user_smartconfig_stop_cb(void *arg) {
	task_t **task = arg;
	if (task == NULL || *task == NULL) {
		return;
	}
	os_free(*task);
	*task = NULL;
}

ESPFUNC static bool user_smartconfig_stop(task_t **task) {
	LOGD(TAG, "smartconfig stop...");
	if (task == NULL || *task == NULL) {
		return false;
	}
	smartconfig_stop();

	smartconfig_task_t *ptask = (smartconfig_task_t *) (*task);
	os_timer_disarm(&ptask->timer);
	os_timer_setfn(&ptask->timer, user_smartconfig_stop_cb, task);
	os_timer_arm(&ptask->timer, 10, 0);
	return true;
}

ESPFUNC static void user_smartconfig_timeout_cb() {
	LOGD(TAG, "smartconfig timeout...");
	wifi_set_opmode(STATION_MODE);
	wifi_station_connect();
}

ESPFUNC void user_smartconfig_instance_start(const task_impl_t *impl, const uint32_t timeout) {
    if (sc_task != NULL) {
        LOGE(TAG, "smartconfig start failed -> already started...");
        return;
    }
    sc_task = os_zalloc(sizeof(smartconfig_task_t));
    if (sc_task == NULL) {
        LOGE(TAG, "smartconfig start failed -> malloc sc_task failed...");
        return;
    }
	LOGD(TAG, "smartconfig create...");
    sc_task->super.vtable = &sc_vtable;
    sc_task->super.impl = impl;
    sc_task->super.timeout = timeout;
    user_task_start((task_t **) &sc_task);
}

ESPFUNC void user_smartconfig_instance_stop() {
    if (sc_task != NULL) {
        user_task_stop((task_t **) &sc_task);
		LOGD(TAG, "sc_task -> %d", sc_task);
    }
}

ESPFUNC bool user_smartconfig_instance_status() {
    return (sc_task != NULL);
}
