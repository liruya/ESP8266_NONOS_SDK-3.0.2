#include "user_apconfig.h"
#include "espconn.h"
#include "cJSON.h"

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

#define CMD_OK          "OK!\n"
#define KEY_SSID        "ssid"
#define KEY_BSSID       "bssid"
#define KEY_PSW         "psw"
#define KEY_ZONE        "zone"
#define KEY_MAC         "mac"
#define KEY_SN          "sn"

#define ACK_TIMEOUT     2000
#define RETRY_COUNT     5

#define SSID_LEN_MIN    8
#define SSID_LEN_MAX    32
#define PSW_LEN_MAX     64

typedef struct {
    bool rcv_valid;                     //通讯接收数据有效
    uint8_t ssid[33];                   //(必需)接收到 要连接的热点SSID
    uint8_t ssid_len;                   //(8~32)接收到的SSID长度
    uint8_t psw[65];                    //(可选)接收到的热点密码
    uint8_t psw_len;                    //(0~64)接收到的热点密码长度
    uint8_t bssid[6];                   //(可选)接收到的热点BSSID
    bool bssid_set;                     //是否接收到有效的热点BSSID
    int16_t zone;                       //(必需)接收到的时区
} apconfig_result_t;

typedef struct {
	task_t super;
	const char *apssid;                  //AP模式下设备SSID
	void (* done)(int16_t);              //AP配网成功回调, 参数为时区

    struct espconn server;              //tcp/udp通讯连接服务端
    uint8_t ack_retry_count;            //通讯命令重发次数计数器
    os_timer_t timer;                   //计时器 通讯响应及停止任务

    apconfig_result_t apc_result;
} apconfig_task_t;

typedef struct {
    ETSEvent        super;
    apconfig_task_t * const pcfg;
} apconfig_event_t;

static const char *TAG = "APConfig";

LOCAL bool user_apconfig_start(task_t **task);
LOCAL bool user_apconfig_stop(task_t **task);
LOCAL void user_apconfig_timeout_cb();

LOCAL apconfig_task_t *apc_task;
LOCAL const task_vtable_t apc_vtable = newTaskVTable(user_apconfig_start, user_apconfig_stop, user_apconfig_timeout_cb);

LOCAL bool ESPFUNC charToHex(char a, char b, uint8_t *hex) {
    uint8_t result = 0;
    if (a >= '0' && a <= '9') {
        result = (a-'0')<<4;
    } else if (a >= 'a' && a <= 'f') {
        result = (a-'a'+10)<<4;
    } else if (a >= 'A' && a <= 'F') {
        result = (a-'A'+10)<<4;
    } else {
        return false;
    }
    if (b >= '0' && b <= '9') {
        result |= (b-'0');
    } else if (b >= 'a' && b <= 'f') {
        result |= (b-'a'+10);
    } else if (b >= 'A' && b <= 'F') {
        result |= (b-'A'+10);
    } else {
        return false;
    }
    *hex = result;
    return true;
}

LOCAL void ESPFUNC user_apconfig_send_info() {
    uint8_t mac[6];
    char macstr[13];
    char snstr[13];
    os_memset(mac, 0, sizeof(mac));
    os_memset(macstr, 0, sizeof(macstr));
    os_memset(snstr, 0, sizeof(snstr));
    if (wifi_get_macaddr(STATION_IF, mac)) {
        os_sprintf(macstr, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        os_sprintf(snstr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        cJSON *root = cJSON_CreateObject();
        if (root == NULL) {
            ERR(TAG, "create json object failed.");
            return;
        }
        cJSON_AddStringToObject(root, KEY_MAC, macstr);
        cJSON_AddStringToObject(root, KEY_SN, snstr);
        char *json = cJSON_PrintUnformatted(root);
        DBG(TAG, "%s", json);
        cJSON_Delete(root);
        if (json != NULL) {
            espconn_send(&apc_task->server, json, os_strlen(json));
            cJSON_free(json);
        }
    }
}

LOCAL void ESPFUNC user_apconfig_station_init(apconfig_result_t *presult) {
    if (presult == NULL) {
        return;
    }
    if (presult->ssid_len < SSID_LEN_MIN || presult->ssid_len > SSID_LEN_MAX || presult->psw_len > PSW_LEN_MAX) {
        return;
    }
    struct station_config config;
    wifi_station_disconnect();
	wifi_set_opmode(STATION_MODE);
	wifi_station_get_config(&config);
	os_memcpy(config.ssid, presult->ssid, presult->ssid_len);
	os_memcpy(config.password, presult->psw, presult->psw_len);
	config.ssid[presult->ssid_len] = '\0';
	config.password[presult->psw_len] = '\0';
	
	config.bssid_set = presult->bssid_set;
	config.threshold.rssi = -127;
	if (presult->psw_len == 0) {
		config.threshold.authmode = AUTH_OPEN;
	} else {
		config.threshold.authmode = AUTH_WPA_WPA2_PSK;
	}
    if (presult->bssid_set) {
        os_memcpy(config.bssid, presult->bssid, 6);
    }
    wifi_station_set_config(&config);
    wifi_station_connect();
}

LOCAL void ESPFUNC user_apconfig_recv_cb(void *arg, char *pdata, uint16_t len) {
    if (apc_task == NULL) {
        return;
    }
    if (pdata == NULL || len == 0) {
        return;
    }
    remot_info info;
	remot_info *premote = &info;
	if (espconn_get_connection_info(&apc_task->server, &premote, 0) != ESPCONN_OK) {
		return;
	}
    DBG(TAG, "%d.%d.%d.%d:%d", premote->remote_ip[0], premote->remote_ip[1], premote->remote_ip[2], premote->remote_ip[3], premote->remote_port);

#ifdef  USE_TCP
    os_memcpy(apc_task->server.proto.tcp->remote_ip, premote->remote_ip, 4);
	apc_task->server.proto.tcp->remote_port = premote->remote_port;
#else
	os_memcpy(apc_task->server.proto.udp->remote_ip, premote->remote_ip, 4);
	apc_task->server.proto.udp->remote_port = premote->remote_port;
#endif

    //decode router info
	char* pdes = os_zalloc(len+1);
    if (pdes == NULL) {
        ERR(TAG, "apconfig error: os_zalloc failed");
        return;
    }
	os_memcpy(pdes, pdata, len);
	pdes[len] = '\0';
    DBG(TAG, "%s", pdes);

    //通讯结束  进入STATION模式
    if (apc_task->apc_result.rcv_valid) {
        if (os_strcmp(pdes, CMD_OK) == 0) {
            os_timer_disarm(&apc_task->timer);
            apc_task->ack_retry_count = 0;

            apconfig_result_t result;
            os_memset(&result, 0, sizeof(apconfig_result_t));
            os_memcpy(&result, &apc_task->apc_result, sizeof(apconfig_result_t));
            user_task_stop((task_t **) &apc_task);
            apc_task->done(result.zone);
            user_apconfig_station_init(&result);
        }
        os_free(pdes);
        return;
    }

    cJSON *root = cJSON_Parse(pdes);
    if (root == NULL) {
        os_free(pdes);
        ERR(TAG, "cJSON parse receive failed...");
        return;
    }

    cJSON *jssid = cJSON_GetObjectItem(root, KEY_SSID);
    if (jssid == NULL || !cJSON_IsString(jssid)) {
        os_free(pdes);
        cJSON_Delete(root);
        ERR(TAG, "ap info without ssid.");
        return;
    }
    char *pssid = jssid->valuestring;
    if (pssid == NULL) {
        os_free(pdes);
        cJSON_Delete(root);
        ERR(TAG, "ap info without ssid..");
        return;
    }
    apc_task->apc_result.ssid_len = os_strlen(pssid);
    if (apc_task->apc_result.ssid_len < SSID_LEN_MIN || apc_task->apc_result.ssid_len > SSID_LEN_MAX) {
        os_free(pdes);
        cJSON_Delete(root);
        ERR(TAG, "ssid len > 32.");
        return;
    }
    DBG(TAG, "ssid: %s: ", pssid);

    cJSON *jzone = cJSON_GetObjectItem(root, KEY_ZONE);
    if (jzone == NULL || !cJSON_IsNumber(jzone)) {
        os_free(pdes);
        cJSON_Delete(root);
        ERR(TAG, "ap info without zone.");
        return;
    }
    int16_t zone = jzone->valueint;
    if (zone > 1200 || zone < -1200 || zone%100 >= 60 || zone%100 <= -60) {
        os_free(pdes);
        cJSON_Delete(root);
        ERR(TAG, "ap info invalid zone.");
        return;
    }
    apc_task->apc_result.zone = zone;
    DBG(TAG, "zone: %d", apc_task->apc_result.zone);

    cJSON *jpsw = cJSON_GetObjectItem(root, KEY_PSW);
    char *ppsw = NULL;
    if (jpsw != NULL && cJSON_IsString(jpsw)) {
        ppsw = jpsw->valuestring;
    }
    if (ppsw != NULL) {
        apc_task->apc_result.psw_len = os_strlen(ppsw);
        if (apc_task->apc_result.psw_len > PSW_LEN_MAX) {
            os_free(pdes);
            cJSON_Delete(root);
            ERR(TAG, "psw len > 64");
            return;
        }
    }

    cJSON *jbssid = cJSON_GetObjectItem(root, KEY_BSSID);
    char *pbssid = NULL;
    if (jbssid != NULL && cJSON_IsString(jbssid)) {
        pbssid = jbssid->valuestring;
    }
    if (pbssid != NULL && os_strlen(pbssid) == 12) {
        apc_task->apc_result.bssid_set = true;
        uint8_t i, s1, s2;
        for (i = 0; i < 6; i++) {
            s1 = *(pbssid+2*i);
            s2 = *(pbssid+2*i+1);
            if (charToHex(s1, s2, &apc_task->apc_result.bssid[i]) == false) {
                apc_task->apc_result.bssid_set = false;
                break;
            }
        }
    }
    os_memcpy(apc_task->apc_result.ssid, pssid, apc_task->apc_result.ssid_len);
    os_memcpy(apc_task->apc_result.psw, ppsw, apc_task->apc_result.psw_len);
    apc_task->apc_result.rcv_valid = true;

    os_free(pdes);
    cJSON_Delete(root);

    DBG(TAG, "ssid: %s\npsw: %s\nbssid: %s\nzone: %d", pssid, ppsw, pbssid, zone);
    user_apconfig_send_info();
    os_timer_disarm(&apc_task->timer);
    apc_task->ack_retry_count = 0;
    os_timer_arm(&apc_task->timer, ACK_TIMEOUT, 1);
}

void ESPFUNC user_apconfig_ack_timeout(void *arg) {
    if (apc_task->ack_retry_count <= RETRY_COUNT) {
        apc_task->ack_retry_count++;
        user_apconfig_send_info();
    } else {
        os_timer_disarm(&apc_task->timer);
        apc_task->ack_retry_count = 0;
    }
}

LOCAL bool ESPFUNC user_apconfig_start(task_t **task) {
    DBG(TAG, "apconfig start... %d  *%d  **%d", task, *task, **task);
    if (task == NULL || *task == NULL) {
        return false;
    }
    apconfig_task_t *ptask = (apconfig_task_t *) (*task);
    struct softap_config config;
    wifi_softap_get_config_default(&config);
    if (os_strcmp(ptask->apssid, config.ssid) != 0) {
        os_strcpy(config.ssid, ptask->apssid);
        config.ssid_len = os_strlen(ptask->apc_result.ssid);
        wifi_softap_set_config(&config);
    }

    ptask->server.state = ESPCONN_NONE;
    ptask->server.reverse = NULL;
#ifdef  USE_TCP
    ptask->server.type = ESPCONN_TCP;
    ptask->server.proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
    ptask->server.proto.tcp->local_port = APCONFIG_LOCAL_PORT;
    espconn_regist_recvcb(&ptask->server, user_apconfig_recv_cb);
    espconn_regist_disconcb(&ptask->server, user_apconfig_disconn_cb);
    int8_t error = espconn_accept(&ptask->server);
    DBG(TAG, "espconn accept error: %d", error);
    espconn_regist_time(&ptask->server, 180, 0);
#else
    ptask->server.type = ESPCONN_UDP;
    ptask->server.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
    ptask->server.proto.udp->local_port = APCONFIG_LOCAL_PORT;
    espconn_regist_recvcb(&ptask->server, user_apconfig_recv_cb);
    int8_t error = espconn_create(&ptask->server);
    DBG(TAG, "espconn create error: %d", error);
#endif

    os_timer_disarm(&ptask->timer);
    os_timer_setfn(&ptask->timer, user_apconfig_ack_timeout, NULL);
    return true;
}

#ifdef  USE_TCP
LOCAL void ESPFUNC user_apconfig_disconn_cb(void *arg) {
    DBG(TAG, "espconn diconnect callback");
}
#endif

LOCAL void ESPFUNC user_apconfig_stop_cb(void *arg) {
    task_t **task = arg;
    if (task == NULL || *task == NULL) {
        return;
    }
    apconfig_task_t *ptask = (apconfig_task_t *) (*task);
    int8_t error = 0;
    #ifdef  USE_TCP
        error = espconn_disconnect(&ptask->server);
        DBG(TAG, "espconn tcp disconnect error: %d", error);
        error = espconn_delete(&ptask->server);
        DBG(TAG, "espconn tcp delete error: %d", error);
        os_free(ptask->server.proto.tcp);
    #else
        error = espconn_delete(&ptask->server);
        DBG(TAG, "espconn udp delete error: %d", error);
        os_free(ptask->server.proto.udp);
    #endif
    os_free(*task);
    *task = NULL;
}

LOCAL bool ESPFUNC user_apconfig_stop(task_t **task) {
    DBG(TAG, "apconfig stop... %d  *%d  **%d", task, *task, **task);
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

LOCAL void ESPFUNC user_apconfig_timeout_cb() {
    DBG(TAG, "apconfig timeout...");
    wifi_set_opmode(STATION_MODE);
    wifi_station_connect();
}

void ESPFUNC user_apconfig_instance_start(const task_impl_t *impl, const uint32_t timeout, const char *ssid, void (* const done)(int16_t)) {
    if (apc_task != NULL) {
        ERR(TAG, "apconfig start failed -> already started...");
        return;
    }
    apc_task = os_zalloc(sizeof(apconfig_task_t));
    if (apc_task == NULL) {
        ERR(TAG, "apconfig start failed -> malloc apc_task failed...");
        return;
    }
    DBG(TAG, "apconfig create... %d", apc_task);
    apc_task->super.vtable = &apc_vtable;
    apc_task->super.impl = impl;
    apc_task->super.timeout = timeout;
    apc_task->apssid = ssid;
    apc_task->done = done;
    user_task_start((task_t **) &apc_task);
}

void ESPFUNC user_apconfig_instance_stop() {
    if (apc_task != NULL) {
        user_task_stop((task_t **) &apc_task);
    }
}

bool ESPFUNC user_apconfig_instance_status() {
    return (apc_task != NULL);
}