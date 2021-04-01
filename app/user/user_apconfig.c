#include "user_apconfig.h"
#include "espconn.h"
#include "cJSON.h"
#include "aliot_defs.h"
#include "user_device.h"

/**
 * AP配网流程
 * 1. 长按按键进入SmartConfig模式, 再次长按进入AP配网模式
 * 2. 手机连接到设备热点
 * 3. APP发送指令获取设备信息
 * 4. 设备返回相应信息
 * 5. APP发送指令设置ssid password zone time
 * 6. 设备返回成功响应, 然后连接到router
 * 7. 手机等待与云平台建立连接后订阅设备 
 * */

// TCP连接存在bug, 设备端及手机端尽在第一次配网时有效
// TCP传输服务在成功后执行espconn_delete失败
// #define USE_TCP

#define	APCONFIG_LOCAL_PORT				8266

#define KEY_SSID        "ssid"
#define KEY_PSW         "password"
#define KEY_ZONE        "zone"
#define KEY_TIME        "time"

#define SSID_LEN_MAX    32
#define PSW_LEN_MIN     8
#define PSW_LEN_MAX     64

typedef struct {
    void (*apc_pre_cb)();
    void (*apc_post_cb)();
    void (*set_time)(int zone, uint64_t time);
} apconfig_callback_t;

static const char *TAG = "apconfig";

static bool status;
static struct espconn server;              //tcp/udp通讯连接服务端
static esp_udp udp;
static os_timer_t timer;                   //计时器 通讯响应及停止任务

static apconfig_callback_t apc_callback;

/* ****************************************************************************
 * 
 * parse receive data
 * 
 * ***************************************************************************/

ICACHE_FLASH_ATTR static void parse_rcv_get(cJSON *request) {
	if (cJSON_IsArray(request) == false) {
		return;
	}
	int size = cJSON_GetArraySize(request);
	if (size <= 0) {
		return;
	}
    cJSON *result = cJSON_CreateObject();
	cJSON *resp = cJSON_CreateObject();
	for (int i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(item)) {
			char *key = item->valuestring;
			char *value = user_device_get(key);
			if (key != NULL && value != NULL) {
				cJSON_AddStringToObject(resp, key, value);
			}
		}
	}
    cJSON_AddItemToObject(result, "get_resp", resp);
    char *payload = cJSON_PrintUnformatted(result);
    espconn_send(&server, payload, os_strlen(payload));
}

static os_timer_t conn_timer;
static struct station_config sta_config;

ICACHE_FLASH_ATTR static void wifi_connect(void *arg) {
    struct station_config *pconfig = (struct station_config *) arg;
    
	wifi_set_opmode(STATION_MODE);
    wifi_station_disconnect();
    wifi_station_set_config(pconfig);
    wifi_station_connect();
}

#define SET_SUCCESS_RESPONSE    "{\"set_resp\":{\"result\":\"success\"}}"
ICACHE_FLASH_ATTR static void parse_rcv_set(cJSON *request) {
	if (cJSON_IsObject(request) == false) {
		return;
	}

    //  check ssid
    cJSON *ssid = cJSON_GetObjectItem(request, KEY_SSID);
	if (!cJSON_IsString(ssid)) {
		return;
	}
    int ssid_len = os_strlen(ssid->valuestring);
    if (ssid_len > SSID_LEN_MAX) {
        return;
    }

    //  check password
    cJSON *password = cJSON_GetObjectItem(request, KEY_PSW);
    if (!cJSON_IsString(password)) {
        return;
    }
    int psw_len = os_strlen(password->valuestring);
    if (psw_len != 0 && psw_len < PSW_LEN_MIN && psw_len > PSW_LEN_MAX) {
        return;
    }

    //  check zone & time
    cJSON *zone = cJSON_GetObjectItem(request, KEY_ZONE);
    if (!cJSON_IsNumber(zone)) {
        return;
    }
    if (zone->valueint < -720 || zone->valueint > 720) {
        return;
    }

    //  check time
    cJSON *time = cJSON_GetObjectItem(request, KEY_TIME);
	if (!cJSON_IsNumber(time)) {
        return;
	}
	LOGD(TAG, "ssid: %s\npsw: %s\nzone: %d", ssid->valuestring, password->valuestring, zone->valueint);

    espconn_send(&server, SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));
    os_delay_us(50000);
    espconn_send(&server, SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));

    if (apc_callback.set_time != NULL) {
        apc_callback.set_time(zone->valueint, (uint64_t) time->valuedouble);
    }

    os_memset(&sta_config, 0, sizeof(sta_config));
    os_strcpy(sta_config.ssid, ssid->valuestring);
    if (psw_len != 0) {
        os_strcpy(sta_config.password, password->valuestring);
        sta_config.threshold.authmode = AUTH_WPA_WPA2_PSK;
    }
    os_timer_disarm(&conn_timer);
    os_timer_setfn(&conn_timer, wifi_connect, &sta_config);
    os_timer_arm(&conn_timer, 1000, 0);

    user_apconfig_stop();
}

ICACHE_FLASH_ATTR static void user_apconfig_parse_rcv(const char *buf) {
	LOGD(TAG, "rcv: %s", buf);
	cJSON *root = cJSON_Parse(buf);
	if (cJSON_IsObject(root) == false) {
		cJSON_Delete(root);
		return;
	}
    cJSON *request;
	if (cJSON_HasObjectItem(root, "get")) {
        request = cJSON_GetObjectItem(root, "get");
		parse_rcv_get(request);
	} else if (cJSON_HasObjectItem(root, "set")) {
		request = cJSON_GetObjectItem(root, "set");
		parse_rcv_set(request);
	}
	cJSON_Delete(root);
}

ICACHE_FLASH_ATTR static void user_apconfig_recv_cb(void *arg, char *pdata, uint16_t len) {
    remot_info info;
	remot_info *premote = &info;
	if (espconn_get_connection_info(&server, &premote, 0) != ESPCONN_OK) {
		return;
	}
    LOGD(TAG, "remote ip: " IPSTR "  port: %d", IP2STR(premote->remote_ip), premote->remote_port);

	os_memcpy(udp.remote_ip, premote->remote_ip, 4);
	udp.remote_port = premote->remote_port;

	char *pbuf = os_zalloc(len+1);
    if (pbuf == NULL) {
        LOGE(TAG, "apconfig error: os_zalloc failed");
        return;
    }
	os_memcpy(pbuf, pdata, len);
    user_apconfig_parse_rcv(pbuf);
    os_free(pbuf);
    pbuf = NULL;
}

ICACHE_FLASH_ATTR void user_apconfig_timeout_cb(void *arg) {
    user_apconfig_stop();
}

ICACHE_FLASH_ATTR bool user_apconfig_start(const char *apssid, const uint32_t timeout, void (* pre_cb)(), void (* post_cb), void (*set_time)(int zone, uint64_t time)) {
    if (status) {
        return false;
    }
    status = true;

    apc_callback.apc_pre_cb = pre_cb;
    apc_callback.apc_post_cb = post_cb;
    apc_callback.set_time = set_time;

    if (apc_callback.apc_pre_cb != NULL) {
        apc_callback.apc_pre_cb();
    }

    os_timer_disarm(&timer);
    os_timer_setfn(&timer, user_apconfig_timeout_cb, NULL);
    os_timer_arm(&timer, timeout, 0);

    wifi_set_opmode_current(SOFTAP_MODE);
    struct softap_config config;
    wifi_softap_get_config(&config);
    os_memset(config.ssid, 0, sizeof(config.ssid));
    if (apssid != NULL && os_strlen(apssid) <= 32) {
        memcpy(config.ssid, apssid, os_strlen(apssid));
        config.ssid_len = os_strlen(config.ssid);
    }
    wifi_softap_set_config_current(&config);

    os_memset(&udp, 0, sizeof(udp));
    udp.local_port  = APCONFIG_LOCAL_PORT;

    os_memset(&server, 0, sizeof(server));
    server.state    = ESPCONN_NONE;
    server.reverse  = NULL;
    server.type     = ESPCONN_UDP;
    server.proto.udp    = &udp;
    espconn_regist_recvcb(&server, user_apconfig_recv_cb);
    int8_t ret = espconn_create(&server);
    LOGD(TAG, "espconn create ret: %d", ret);

    return true;
}

ICACHE_FLASH_ATTR void user_apconfig_stop() {
    os_timer_disarm(&timer);

    int8_t ret = espconn_delete(&server);
    LOGD(TAG, "espconn delete ret: %d", ret);

    uint8_t mode = wifi_get_opmode_default();
    if (mode != SOFTAP_MODE) {
        wifi_set_opmode_current(mode);
        if (mode == STATION_MODE) {
            wifi_station_connect();
        }
    }
    status = false;

    if (apc_callback.apc_post_cb != NULL) {
        apc_callback.apc_post_cb();
    }
}

ICACHE_FLASH_ATTR bool user_apconfig_status() {
    return status;
}