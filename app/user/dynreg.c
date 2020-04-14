#include "dynreg.h"
#include "aliot_sign.h"
#include "ca_cert.h"
#include "mqtt_config.h"

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

typedef struct {
	dynreg_success_cb_t dynreg_success_cb;
} dynreg_func_t;

struct espconn client;
ip_addr_t ipaddr;
os_timer_t timer;
uint32_t retry_cnt;
char *host;
dynreg_func_t dynreg_fun;

ICACHE_FLASH_ATTR void dns_found(const char *name, ip_addr_t *ip, void *arg) {	
	if (ip == NULL || ip->addr == 0) {
		return;
	}
	os_printf("dns found: " IPSTR "\n", IP2STR(ip));

	os_timer_disarm(&timer);
	os_free(host);
	host = NULL;

	os_memcpy(client.proto.tcp->remote_ip, &ip->addr, 4);
	espconn_secure_connect(&client);
}

ICACHE_FLASH_ATTR void dns_retry_timer_cb(void *arg) {
	if (retry_cnt < 4) {
		retry_cnt++;
	} else {
		retry_cnt = 4;
	}
	uint32_t period = (1<<retry_cnt)*4000;

	ipaddr.addr = 0;
	espconn_gethostbyname(&client, host, &ipaddr, dns_found);
	os_timer_arm(&timer, period, 0);
}

ICACHE_FLASH_ATTR void timer_disconnect_cb(void *arg) {
	espconn_secure_disconnect(&client);
}

ICACHE_FLASH_ATTR void send_dynreg_request(dev_meta_info_t *meta) {
	//	random
	char temp[7];
	char random[15];
	os_memset(random, 0, sizeof(random));
	os_get_random(temp, sizeof(temp));
	os_sprintf(random, "%02X%02X%02X%02X%02X%02X%02X", temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6]);
    os_printf("random: %s\n", random);

	int sign_source_len = 0;
    char *sign_source = NULL;
	aliot_sign_t *psign = aliot_sign_get();

    sign_source_len = os_strlen(DYNREG_SIGNSOURCE_FMT) + os_strlen(meta->device_name) + os_strlen(meta->product_key) + os_strlen(random) + 1;
    sign_source = os_zalloc(sign_source_len);
    if (sign_source == NULL) {
        os_printf("malloc sign_source failed.\n");
        return;
    }
    os_snprintf(sign_source, sign_source_len, DYNREG_SIGNSOURCE_FMT, meta->device_name, meta->product_key, random);

	char signout[65];
	os_memset(signout, 0 , sizeof(signout));
    psign->function(sign_source, os_strlen(sign_source), meta->product_secret, os_strlen(meta->product_secret), signout);
    os_free(sign_source);
    os_printf("sign: %s\n", signout);

	int datlen = os_strlen(DYNREG_REGDATA_FMT) + os_strlen(meta->product_key) + os_strlen(meta->device_name) + os_strlen(random) + os_strlen(signout) + os_strlen(psign->name) + 1;
	char *data = os_zalloc(datlen);
	if (data == NULL) {
		os_printf("malloc data failed.\n");
		return;
	}
	os_snprintf(data, datlen, DYNREG_REGDATA_FMT, meta->product_key, meta->device_name, random, signout, psign->name);

	int len = os_strlen(DYNREG_REGISTER_FMT) + os_strlen(meta->region) + 8 + os_strlen(data);
	char *buf = os_zalloc(len);
	if (buf == NULL) {
		os_printf("malloc buf failed.\n");
		os_free(data);
		return;
	}
	os_snprintf(buf, len, DYNREG_REGISTER_FMT, meta->region, os_strlen(data), data);
	os_printf("%s\n", buf);
	espconn_secure_send(&client, buf, os_strlen(buf));

	os_free(data);
	os_free(buf);
}

ICACHE_FLASH_ATTR void parse_dynreg_result(const char *payload) {
    char *pkCode = os_strstr(payload, "{\"code\":");
    if (pkCode == NULL) {
        os_printf("dynreg failed.\n");
        return;
    }
    char *pkCodeSuc = os_strstr(pkCode, "{\"code\":200");
    char *pkData = os_strstr(pkCode, "\"data\":");
    if (pkCodeSuc == NULL || pkData == NULL) {
        os_printf("dynreg failed. --> %s\n", pkCode);
        return;
    }
    char *pkSecret = os_strstr(pkData, "\"deviceSecret\":");
    char *pStart = os_strchr(pkSecret + os_strlen("\"deviceSecret\":"), '\"');
    char *pEnd = os_strchr(pStart+1, '\"');
    if (pkSecret == NULL || pStart == NULL || pEnd == NULL || pEnd - pStart > DEVICE_SECRET_LEN + 1) {
        os_printf("dynreg failed. no deviceSecret.\n");
        return;
    }
    char secret[DEVICE_SECRET_LEN+1];
    os_memset(secret, 0, sizeof(secret));
    os_memcpy(secret, pStart+1, pEnd - pStart - 1);
    os_printf("deviceSecret: %s\n", secret);
	if (dynreg_fun.dynreg_success_cb != NULL) {
		dynreg_fun.dynreg_success_cb(secret);
	}
}

ICACHE_FLASH_ATTR void connect_cb(void *arg) {
	os_printf("connect...\n");
	send_dynreg_request(client.reverse);
}

ICACHE_FLASH_ATTR void disconnect_cb(void *arg) {
    os_printf("disconnect...\n");
    espconn_secure_ca_disable(ESPCONN_CLIENT);
    espconn_secure_set_size(ESPCONN_CLIENT, 2048);
}

ICACHE_FLASH_ATTR void reconnect_cb(void *arg, int8_t err) {
    os_printf("reconnect...\n");
}

ICACHE_FLASH_ATTR void sent_cb(void *arg) {
    os_printf("sent...\n");
}

ICACHE_FLASH_ATTR void recv_cb(void *arg, char *pdata, uint16_t len) {
    char *buf = os_zalloc(len+1);
    os_memcpy(buf, pdata, len);
    os_printf("recv: %s\n", buf);
    parse_dynreg_result(buf);
    os_free(buf);
	buf = NULL;

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, timer_disconnect_cb, NULL);
	os_timer_arm(&timer, 20, 0);
}

ICACHE_FLASH_ATTR void dynreg_start(dev_meta_info_t *meta, dynreg_success_cb_t success_cb) {
	check_ca_bin();
	if (client.proto.tcp == NULL) {
		client.proto.tcp = (esp_tcp *) os_zalloc(sizeof(esp_tcp));
		if (client.proto.tcp == NULL) {
			os_printf("malloc tcp failed.\n");
			return;
		}
	}

	dynreg_fun.dynreg_success_cb = success_cb;

	client.type = ESPCONN_TCP;
	client.state = ESPCONN_NONE;
	client.proto.tcp->local_port = espconn_port();
	client.proto.tcp->remote_port = 443;
	client.reverse = meta;
	espconn_secure_set_size(ESPCONN_CLIENT, TLS_CACHE_SIZE);
	// espconn_secure_ca_enable(ESPCONN_CLIENT, CA_SECTOR_ADDR);
	espconn_regist_connectcb(&client, connect_cb);
	espconn_regist_disconcb(&client, disconnect_cb);
	espconn_regist_sentcb(&client, sent_cb);
	espconn_regist_recvcb(&client, recv_cb);

	host = os_zalloc(os_strlen(DYNREG_HOSTNAME_FMT) + os_strlen(meta->region) + 1);
	os_sprintf(host, DYNREG_HOSTNAME_FMT, meta->region);

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, dns_retry_timer_cb, NULL);

	ipaddr.addr = 0;
	espconn_gethostbyname(&client, host, &ipaddr, dns_found);
	os_timer_arm(&timer, 4000, 0);
}

