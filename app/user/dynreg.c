#include "dynreg.h"
#include "aliot_sign.h"
#include "ca_cert.h"
#include "mqtt_config.h"
#include "app_common.h"
#include "cJSON.h"

//	"iot-auth.${region}.aliyuncs.com"
#define	DYNREG_HOSTNAME_FMT			"iot-auth.%s.aliyuncs.com"

//	deviceName${deviceName}productKey${productKey}random${random}
#define	DYNREG_SIGNSOURCE_FMT		"deviceName%sproductKey%srandom%s"	

//	${region}	${data_len}
#define DYNREG_REGISTER_FMT     	"POST /auth/register/device  HTTP/1.1\r\n\
Host: iot-auth.%s.aliyuncs.com\r\n\
Accept: text/xml,text/javascript,text/html,application/json\r\n\
Content-Type: application/x-www-form-urlencoded\r\n\
Content-Length: %d\r\n\r\n\
%s"

//	${productKey} ${deviceName} ${random} ${sign} ${signMethod}
#define	DYNREG_REGDATA_FMT			"productKey=%s&deviceName=%s&random=%s&sign=%s&signMethod=%s"

static const char *TAG = "dynreg";

typedef struct {
	dynreg_success_cb_t dynreg_success_cb;
} dynreg_func_t;


static os_timer_t timer;
static uint32_t retry_cnt;
dynreg_func_t dynreg_fun;

ICACHE_FLASH_ATTR void dns_found(const char *name, ip_addr_t *ip, void *arg) {
	struct espconn *pespconn = (struct espconn *) arg;
	if (ip == NULL || ip->addr == 0) {
		return;
	}
	LOGD(TAG, "name: %s, dns found: " IPSTR "\n", name, IP2STR(ip));

	os_timer_disarm(&timer);

	os_memcpy(pespconn->proto.tcp->remote_ip, &ip->addr, 4);
	espconn_secure_connect(pespconn);
}

ICACHE_FLASH_ATTR void dns_retry_timer_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *) arg;
	dev_meta_info_t *meta = pespconn->reverse;

	if (retry_cnt < 4) {
		retry_cnt++;
	} else {
		retry_cnt = 4;
	}
	uint32_t period = retry_cnt*4000;

	char host[80] = {0};
	os_sprintf(host, DYNREG_HOSTNAME_FMT, meta->region);

	ip_addr_t ipaddr;
	espconn_gethostbyname(pespconn, host, &ipaddr, dns_found);
	os_timer_arm(&timer, period, 0);
}

ICACHE_FLASH_ATTR void timer_disconnect_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *) arg;
	espconn_secure_disconnect(pespconn);
}

ICACHE_FLASH_ATTR void send_dynreg_request(struct espconn *pespconn) {
	dev_meta_info_t *meta = pespconn->reverse;
	//	random
	char temp[7];
	char random[15];
	os_memset(random, 0, sizeof(random));
	os_get_random(temp, sizeof(temp));
	os_sprintf(random, "%02X%02X%02X%02X%02X%02X%02X", temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6]);
    LOGD(TAG, "random: %s\n", random);

	int sign_source_len = 0;
    char *sign_source = NULL;
	aliot_sign_t *psign = aliot_sign_get();

    sign_source_len = os_strlen(DYNREG_SIGNSOURCE_FMT) + os_strlen(meta->device_name) + os_strlen(meta->product_key) + os_strlen(random) + 1;
    sign_source = os_zalloc(sign_source_len);
    if (sign_source == NULL) {
        LOGE(TAG, "malloc sign_source failed.\n");
        return;
    }
    os_snprintf(sign_source, sign_source_len, DYNREG_SIGNSOURCE_FMT, meta->device_name, meta->product_key, random);

	char signout[65];
	os_memset(signout, 0 , sizeof(signout));
    psign->function(sign_source, os_strlen(sign_source), meta->product_secret, os_strlen(meta->product_secret), signout);
    os_free(sign_source);
    LOGD(TAG, "sign: %s\n", signout);

	int datlen = os_strlen(DYNREG_REGDATA_FMT) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + os_strlen(random) + os_strlen(signout) + os_strlen(psign->name) + 1;
	char *data = os_zalloc(datlen);
	if (data == NULL) {
		LOGE(TAG, "malloc data failed.\n");
		return;
	}
	os_snprintf(data, datlen, DYNREG_REGDATA_FMT, meta->product_key, meta->device_name, random, signout, psign->name);

	int len = os_strlen(DYNREG_REGISTER_FMT) + os_strlen(meta->region) + 8 + os_strlen(data);
	char *buf = os_zalloc(len);
	if (buf == NULL) {
		LOGE(TAG, "malloc buf failed.\n");
		os_free(data);
		return;
	}
	os_snprintf(buf, len, DYNREG_REGISTER_FMT, meta->region, os_strlen(data), data);
	LOGD(TAG, "%s\n", buf);
	espconn_secure_send(pespconn, buf, os_strlen(buf));

	os_free(data);
	os_free(buf);
}

ICACHE_FLASH_ATTR void connect_cb(void *arg) {
	struct espconn *pespconn = (struct espconn *) arg;
	LOGD(TAG, "connect...\n");
	send_dynreg_request(pespconn);
}

ICACHE_FLASH_ATTR void disconnect_cb(void *arg) {
    LOGD(TAG, "disconnect...\n");
    espconn_secure_ca_disable(ESPCONN_CLIENT);
    espconn_secure_set_size(ESPCONN_CLIENT, 2048);
}

ICACHE_FLASH_ATTR void reconnect_cb(void *arg, int8_t err) {
    LOGD(TAG, "reconnect...\n");
}

ICACHE_FLASH_ATTR void sent_cb(void *arg) {
    LOGD(TAG, "sent...\n");
}

ICACHE_FLASH_ATTR void recv_cb(void *arg, char *pdata, uint16_t len) {
	struct espconn *pespconn = (struct espconn *) arg;

	LOGD(TAG, "%s", pdata);

	char *resp = os_strstr(pdata, "\r\n\r\n");
	if (resp != NULL) {
		cJSON *root = cJSON_ParseWithOpts(resp + os_strlen("\r\n\r\n"), 0, 0);
		do {
			if (cJSON_IsObject(root) == false) {
				break;
			}
			cJSON *code = cJSON_GetObjectItem(root, "code");
			if (cJSON_IsNumber(code) == false || code->valueint != 200) {
				break;
			}
			cJSON *data = cJSON_GetObjectItem(root, "data");
			if (cJSON_IsObject(data) == false) {
				break;
			}
			cJSON *dsecret = cJSON_GetObjectItem(data, "deviceSecret");
			if (cJSON_IsString(dsecret) == false) {
				break;
			}
			char *secret = dsecret->valuestring;
			LOGD(TAG, "deviceSecret: %s\n", secret);
			if (dynreg_fun.dynreg_success_cb != NULL) {
				dynreg_fun.dynreg_success_cb(secret);
			}
		} while (0);
		cJSON_Delete(root);
	}

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, timer_disconnect_cb, arg);
	os_timer_arm(&timer, 20, 0);
}

ICACHE_FLASH_ATTR void dynreg_start(dev_meta_info_t *meta, dynreg_success_cb_t success_cb) {
	static esp_tcp tcp;
	static struct espconn client;
	check_ca_bin();
	
	dynreg_fun.dynreg_success_cb = success_cb;

	os_memset(&tcp, 0, sizeof(tcp));
	tcp.local_port = espconn_port();
	tcp.remote_port = 443;

	os_memset(&client, 0, sizeof(client));
	client.type = ESPCONN_TCP;
	client.state = ESPCONN_NONE;
	client.proto.tcp = &tcp;
	client.reverse = meta;
	espconn_secure_set_size(ESPCONN_CLIENT, TLS_CACHE_SIZE);
	// espconn_secure_ca_enable(ESPCONN_CLIENT, CA_SECTOR_ADDR);
	espconn_regist_connectcb(&client, connect_cb);
	espconn_regist_disconcb(&client, disconnect_cb);
	espconn_regist_sentcb(&client, sent_cb);
	espconn_regist_recvcb(&client, recv_cb);

	char host[80] = {0};
	os_sprintf(host, DYNREG_HOSTNAME_FMT, meta->region);

	ip_addr_t ipaddr;
	espconn_gethostbyname(&client, host, &ipaddr, dns_found);

	retry_cnt = 0;
	os_timer_disarm(&timer);
	os_timer_setfn(&timer, dns_retry_timer_cb, &client);
	os_timer_arm(&timer, 4000, 0);
}

