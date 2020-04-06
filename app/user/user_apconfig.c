#include "user_apconfig.h"
#include "espconn.h"
#include "cJSON.h"
#include "aliot_defs.h"
#include "wrappers_product.h"

/**
 * AP配网流程
 * 1. 设备恢复出厂设置后，进入SoftAP模式，手机连接到设备热点
 * 2. 手机发送路由器ssid + password等信息到设备
 * 3. 设备收到路由器ssid和password后返回MAC SN等信息
 * 4. 手机收到MAC SN等信息后发送通讯结束命令
 * 5. 设备收到通讯结束命令后开启允许二维码/SN订阅, 进入STATION模式
 * 6. 手机等待与云平台建立连接后通过SN订阅设备 
 * */

// TCP连接存在bug, 设备端及手机端尽在第一次配网时有效
// TCP传输服务在成功后执行espconn_delete失败
// #define USE_TCP

#define	APCONFIG_LOCAL_PORT				8266

#define KEY_SSID        "ssid"
#define KEY_PSW         "password"
#define KEY_ZONE        "zone"
#define KEY_TIME        "time"

#define SSID_LEN_MIN    8
#define SSID_LEN_MAX    32
#define PSW_LEN_MIN     8
#define PSW_LEN_MAX     64

typedef struct {
	task_t super;
	const char *apssid;                  //AP模式下设备SSID
	void (* done)(int, long);                  //AP配网成功回调, 参数为时区

    struct espconn server;              //tcp/udp通讯连接服务端
    os_timer_t timer;                   //计时器 通讯响应及停止任务
} apconfig_task_t;

static const char *TAG = "APConfig";

static bool user_apconfig_start(task_t **task);
static bool user_apconfig_stop(task_t **task);
static void user_apconfig_timeout_cb();

static apconfig_task_t *apc_task;
static const task_vtable_t apc_vtable = newTaskVTable(user_apconfig_start, user_apconfig_stop, user_apconfig_timeout_cb);


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
	int i;
	char *buf = os_zalloc(256);
	if (buf == NULL) {
		return;
	}
	int cnt = 0;
    os_sprintf(buf, "{\"get_resp\":{");
	for (i = 0; i < size; i++) {
		cJSON *item = cJSON_GetArrayItem(request, i);
		if (cJSON_IsString(item)) {
			char *key = item->valuestring;
			char *value = hal_product_get(key);
			if (key != NULL && value != NULL) {
				os_sprintf(buf+os_strlen(buf), "\"%s\":\"%s\",", key, value);
				cnt++;
			}
		}
	}
	if (cnt != 0) {
		int len = os_strlen(buf);
		buf[len-1] = '}';
        buf[len] = '}';
		LOGD(TAG, "response: %s", buf);
        espconn_send(&apc_task->server, buf, len+1);
	}
	os_free(buf);
	buf = NULL;
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
    if (ssid_len < SSID_LEN_MIN || ssid_len > SSID_LEN_MAX) {
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
    apc_task->done(zone->valueint, (uint64_t) time->valuedouble);

	LOGD(TAG, "ssid: %s\npsw: %s\nzone: %d", ssid->valuestring, password->valuestring, zone->valueint);

    espconn_send(&apc_task->server, SET_SUCCESS_RESPONSE, os_strlen(SET_SUCCESS_RESPONSE));

    os_delay_us(50000);
    struct station_config config;
    os_memset(&config, 0, sizeof(config));
    os_strcpy(config.ssid, ssid->valuestring);
    if (psw_len != 0) {
        os_strcpy(config.password, password->valuestring);
        config.threshold.authmode = AUTH_WPA_WPA2_PSK;
    }
    wifi_station_disconnect();
	wifi_set_opmode(STATION_MODE);
    wifi_station_set_config(&config);
    wifi_station_connect();

    // os_memset(&apc_task->apc_result, 0, sizeof(apc_task->apc_result));
    // os_strcpy(apc_task->apc_result.ssid, ssid->valuestring);
    // apc_task->apc_result.ssid_len = ssid_len;
    // os_strcpy(apc_task->apc_result.psw, password->valuestring);
    // apc_task->apc_result.psw_len = psw_len;
    // apc_task->apc_result.zone = zone->valueint;
    // apc_task->apc_result.rcv_valid = true;

    // user_apconfig_send_info();
    // os_timer_disarm(&apc_task->timer);
    // apc_task->ack_retry_count = 0;
    // os_timer_arm(&apc_task->timer, ACK_TIMEOUT, 1);
}

ICACHE_FLASH_ATTR static void user_apconfig_parse_rcv(const char *buf) {
	LOGD(TAG, "rcv: %s", buf);
	cJSON *root = cJSON_Parse(buf);
	if (cJSON_IsObject(root) == false) {
		cJSON_Delete(root);
		return;
	}
	if (cJSON_HasObjectItem(root, "get")) {
		cJSON *getReq = cJSON_GetObjectItem(root, "get");
		parse_rcv_get(getReq);
	} else if (cJSON_HasObjectItem(root, "set")) {
		cJSON *setReq = cJSON_GetObjectItem(root, "set");
		parse_rcv_set(setReq);
	}
	cJSON_Delete(root);
}

// ICACHE_FLASH_ATTR static void user_apconfig_send_info() {
//     uint8_t mac[6];
//     char macstr[13];
//     char snstr[13];
//     os_memset(mac, 0, sizeof(mac));
//     os_memset(macstr, 0, sizeof(macstr));
//     os_memset(snstr, 0, sizeof(snstr));
//     if (wifi_get_macaddr(STATION_IF, mac)) {
//         os_sprintf(macstr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//         os_sprintf(snstr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
//         cJSON *root = cJSON_CreateObject();
//         if (root == NULL) {
//             LOGE(TAG, "create json object failed.");
//             return;
//         }
//         cJSON_AddStringToObject(root, KEY_MAC, macstr);
//         cJSON_AddStringToObject(root, KEY_SN, snstr);
//         char *json = cJSON_PrintUnformatted(root);
//         LOGD(TAG, "%s", json);
//         cJSON_Delete(root);
//         if (json != NULL) {
//             espconn_send(&apc_task->server, json, os_strlen(json));
//             cJSON_free(json);
//         }
//     }
// }

// ICACHE_FLASH_ATTR static void user_apconfig_station_init(apconfig_result_t *presult) {
//     if (presult == NULL) {
//         return;
//     }
//     if (presult->ssid_len < SSID_LEN_MIN || presult->ssid_len > SSID_LEN_MAX || presult->psw_len > PSW_LEN_MAX) {
//         return;
//     }
//     struct station_config config;
//     wifi_station_disconnect();
// 	wifi_set_opmode(STATION_MODE);
// 	wifi_station_get_config(&config);
// 	os_memcpy(config.ssid, presult->ssid, presult->ssid_len);
// 	os_memcpy(config.password, presult->psw, presult->psw_len);
// 	config.ssid[presult->ssid_len] = '\0';
// 	config.password[presult->psw_len] = '\0';
	
// 	if (presult->psw_len == 0) {
// 		config.threshold.authmode = AUTH_OPEN;
// 	} else {
// 		config.threshold.authmode = AUTH_WPA_WPA2_PSK;
// 	}
//     wifi_station_set_config(&config);
//     wifi_station_connect();
// }

ICACHE_FLASH_ATTR static void user_apconfig_recv_cb(void *arg, char *pdata, uint16_t len) {
    if (apc_task == NULL) {
        return;
    }
    if (arg == NULL || pdata == NULL || len == 0) {
        return;
    }
    remot_info info;
	remot_info *premote = &info;
	if (espconn_get_connection_info(&apc_task->server, &premote, 0) != ESPCONN_OK) {
		return;
	}
    LOGD(TAG, "remote ip: " IPSTR "  port: %d", IP2STR(premote->remote_ip), premote->remote_port);

#ifdef  USE_TCP
    os_memcpy(apc_task->server.proto.tcp->remote_ip, premote->remote_ip, 4);
	apc_task->server.proto.tcp->remote_port = premote->remote_port;
#else
	os_memcpy(apc_task->server.proto.udp->remote_ip, premote->remote_ip, 4);
	apc_task->server.proto.udp->remote_port = premote->remote_port;
#endif

	char* pbuf = os_zalloc(len+1);
    if (pbuf == NULL) {
        LOGE(TAG, "apconfig error: os_zalloc failed");
        return;
    }
	os_memcpy(pbuf, pdata, len);
    user_apconfig_parse_rcv(pbuf);
    os_free(pbuf);
    pbuf = NULL;

    // //通讯结束  进入STATION模式
    // if (apc_task->apc_result.rcv_valid) {
    //     if (os_strcmp(pdes, CMD_OK) == 0) {
    //         os_timer_disarm(&apc_task->timer);
    //         apc_task->ack_retry_count = 0;

    //         apconfig_result_t result;
    //         os_memset(&result, 0, sizeof(apconfig_result_t));
    //         os_memcpy(&result, &apc_task->apc_result, sizeof(apconfig_result_t));
    //         user_task_stop((task_t **) &apc_task);
    //         apc_task->done(result.zone);
    //         user_apconfig_station_init(&result);
    //     }
    //     os_free(pdes);
    //     return;
    // }

    // cJSON *root = cJSON_Parse(pdes);
    // if (root == NULL) {
    //     os_free(pdes);
    //     LOGE(TAG, "cJSON parse receive failed...");
    //     return;
    // }

    // cJSON *jssid = cJSON_GetObjectItem(root, KEY_SSID);
    // if (!cJSON_IsString(jssid)) {
    //     os_free(pdes);
    //     cJSON_Delete(root);
    //     LOGE(TAG, "ap info without ssid.");
    //     return;
    // }
    // char *pssid = jssid->valuestring;
    // if (pssid == NULL) {
    //     os_free(pdes);
    //     cJSON_Delete(root);
    //     LOGE(TAG, "ap info without ssid..");
    //     return;
    // }
    // apc_task->apc_result.ssid_len = os_strlen(pssid);
    // if (apc_task->apc_result.ssid_len < SSID_LEN_MIN || apc_task->apc_result.ssid_len > SSID_LEN_MAX) {
    //     os_free(pdes);
    //     cJSON_Delete(root);
    //     LOGE(TAG, "ssid len > 32.");
    //     return;
    // }
    // LOGD(TAG, "ssid: %s: ", pssid);

    // cJSON *jzone = cJSON_GetObjectItem(root, KEY_ZONE);
    // if (!cJSON_IsNumber(jzone)) {
    //     os_free(pdes);
    //     cJSON_Delete(root);
    //     LOGE(TAG, "ap info without zone.");
    //     return;
    // }
    // int zone = jzone->valueint;
    // if (zone > 720 || zone < -720) {
    //     os_free(pdes);
    //     cJSON_Delete(root);
    //     LOGE(TAG, "ap info invalid zone.");
    //     return;
    // }
    // apc_task->apc_result.zone = zone;
    // LOGD(TAG, "zone: %d", apc_task->apc_result.zone);

    // cJSON *jpsw = cJSON_GetObjectItem(root, KEY_PSW);
    // char *ppsw = NULL;
    // if (cJSON_IsString(jpsw)) {
    //     ppsw = jpsw->valuestring;
    // }
    // if (ppsw != NULL) {
    //     apc_task->apc_result.psw_len = os_strlen(ppsw);
    //     if (apc_task->apc_result.psw_len > PSW_LEN_MAX) {
    //         os_free(pdes);
    //         cJSON_Delete(root);
    //         LOGE(TAG, "psw len > 64");
    //         return;
    //     }
    // }

    // os_memcpy(apc_task->apc_result.ssid, pssid, apc_task->apc_result.ssid_len);
    // os_memcpy(apc_task->apc_result.psw, ppsw, apc_task->apc_result.psw_len);
    // apc_task->apc_result.rcv_valid = true;

    // os_free(pdes);
    // cJSON_Delete(root);

    // LOGD(TAG, "ssid: %s\npsw: %s\nzone: %d", pssid, ppsw, zone);
    // user_apconfig_send_info();
    // os_timer_disarm(&apc_task->timer);
    // apc_task->ack_retry_count = 0;
    // os_timer_arm(&apc_task->timer, ACK_TIMEOUT, 1);
}

// ICACHE_FLASH_ATTR void user_apconfig_ack_timeout(void *arg) {
//     if (apc_task->ack_retry_count <= RETRY_COUNT) {
//         apc_task->ack_retry_count++;
//         user_apconfig_send_info();
//     } else {
//         os_timer_disarm(&apc_task->timer);
//         apc_task->ack_retry_count = 0;
//     }
// }

ICACHE_FLASH_ATTR static bool user_apconfig_start(task_t **task) {
    LOGD(TAG, "apconfig start...");
    if (task == NULL || *task == NULL) {
        return false;
    }
    apconfig_task_t *ptask = (apconfig_task_t *) (*task);
    struct softap_config config;
    wifi_softap_get_config(&config);
    os_memset(config.ssid, 0, sizeof(config.ssid));
    os_strcpy(config.ssid, ptask->apssid);
    config.ssid_len = os_strlen(config.ssid);
    wifi_softap_set_config_current(&config);

    ptask->server.state = ESPCONN_NONE;
    ptask->server.reverse = NULL;
#ifdef  USE_TCP
    ptask->server.type = ESPCONN_TCP;
    ptask->server.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    ptask->server.proto.tcp->local_port = APCONFIG_LOCAL_PORT;
    espconn_regist_recvcb(&ptask->server, user_apconfig_recv_cb);
    espconn_regist_disconcb(&ptask->server, user_apconfig_disconn_cb);
    int8_t error = espconn_accept(&ptask->server);
    LOGD(TAG, "espconn accept error: %d", error);
    espconn_regist_time(&ptask->server, 180, 0);
#else
    ptask->server.type = ESPCONN_UDP;
    ptask->server.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    ptask->server.proto.udp->local_port = APCONFIG_LOCAL_PORT;
    espconn_regist_recvcb(&ptask->server, user_apconfig_recv_cb);
    int8_t error = espconn_create(&ptask->server);
    LOGD(TAG, "espconn create error: %d", error);
#endif

    // os_timer_disarm(&ptask->timer);
    // os_timer_setfn(&ptask->timer, user_apconfig_ack_timeout, NULL);
    return true;
}

#ifdef  USE_TCP
ICACHE_FLASH_ATTR static void user_apconfig_disconn_cb(void *arg) {
    LOGD(TAG, "espconn diconnect callback");
}
#endif

ICACHE_FLASH_ATTR static void user_apconfig_stop_cb(void *arg) {
    task_t **task = arg;
    if (task == NULL || *task == NULL) {
        return;
    }
    apconfig_task_t *ptask = (apconfig_task_t *) (*task);
    int8_t error = 0;
    #ifdef  USE_TCP
        error = espconn_disconnect(&ptask->server);
        LOGD(TAG, "espconn tcp disconnect error: %d", error);
        error = espconn_delete(&ptask->server);
        LOGD(TAG, "espconn tcp delete error: %d", error);
        os_free(ptask->server.proto.tcp);
    #else
        error = espconn_delete(&ptask->server);
        LOGD(TAG, "espconn udp delete error: %d", error);
        os_free(ptask->server.proto.udp);
    #endif
    os_free(*task);
    *task = NULL;
}

ICACHE_FLASH_ATTR static bool user_apconfig_stop(task_t **task) {
    LOGD(TAG, "apconfig stop...");
    if (task == NULL || *task == NULL) {
        return false;
    }
    apconfig_task_t *ptask = (apconfig_task_t *) (*task);
    os_timer_disarm(&ptask->timer);

    //需要espconn_disconnect和espconn_delete,本方法实际在espconn_recv回调中执行,
    //而espconn_disconnet不允许在espconn任何回调中执行
    os_timer_setfn(&ptask->timer, user_apconfig_stop_cb, task);
    os_timer_arm(&ptask->timer, 10, 0);

    return true;
}

ICACHE_FLASH_ATTR static void user_apconfig_timeout_cb() {
    LOGD(TAG, "apconfig timeout...");
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();
}

ICACHE_FLASH_ATTR void user_apconfig_instance_start(const task_impl_t *impl, const uint32_t timeout, const char *ssid, void (* const done)(int, long)) {
    if (apc_task != NULL) {
        LOGE(TAG, "apconfig start failed -> already started...");
        return;
    }
    apc_task = os_zalloc(sizeof(apconfig_task_t));
    if (apc_task == NULL) {
        LOGE(TAG, "apconfig start failed -> malloc apc_task failed...");
        return;
    }
    LOGD(TAG, "apconfig create...");
    apc_task->super.vtable = &apc_vtable;
    apc_task->super.impl = impl;
    apc_task->super.timeout = timeout;
    apc_task->apssid = ssid;
    apc_task->done = done;
    user_task_start((task_t **) &apc_task);
}

ICACHE_FLASH_ATTR void user_apconfig_instance_stop() {
    if (apc_task != NULL) {
        user_task_stop((task_t **) &apc_task);
    }
}

ICACHE_FLASH_ATTR bool user_apconfig_instance_status() {
    return (apc_task != NULL);
}
