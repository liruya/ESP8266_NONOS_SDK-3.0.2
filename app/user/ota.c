#include "ota.h"
#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "espconn.h"
#include "upgrade.h"
#include "user_device.h"
#include <stdlib.h>

typedef struct {
	char host[120];
	uint16_t port;
	char path[256];
} ota_info_t;

typedef struct {
	void (*progress_cb)(const int step, const char *msg);
} ota_callback_t;

#define	UPGRADE_TIMEOUT		300000	

#define HEADER_FMT 			"GET %s HTTP/1.1\r\n\
Host: %s:%d\r\n\
Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"

#define	DNS_TIMEOUT_PERIOD		5000
#define	RESTART_DELAY			2000

#define	STEP_UPGRADE_FAILED		-1
#define	STEP_DOWNLOAD_FAILED	-2
#define	STEP_VERIFY_FAILED		-3
#define	STEP_FLASH_FAILED		-4
#define	STEP_UPGRADE_START		0
#define	STEP_UPGRADE_SUCCESS	100
#define	ERROR_TIMEOUT			"timeout"
#define	ERROR_INVALID_VERSION	"invalid version"
#define	ERROR_INVALID_URL		"invalid url"
#define	ERROR_URL_TOO_LONG		"url too long"
#define	ERROR_GETIP_FAILED		"get ip failed"
#define	ERROR_UPGRADING			"device is upgrading"
#define	ERROR_UPGRADE_FAILED	"upgrade failed"

static const char *TAG = "OTA";

static ip_addr_t ipaddr;
static struct upgrade_server_info *server;
static os_timer_t timer;
static bool processing;
static ota_callback_t ota_callback;

ICACHE_FLASH_ATTR void ota_deinit() {
	if (server != NULL) {
		if (server->url != NULL) {
			os_free(server->url);
		}
		if (server->pespconn != NULL) {
			os_free(server->pespconn);
		}
		os_free(server);
	}
	processing = false;
}

ICACHE_FLASH_ATTR void ota_upgrade_response(const int step, const char *msg) {
	if (ota_callback.progress_cb != NULL) {
		ota_callback.progress_cb(step, msg);
	}
}

ICACHE_FLASH_ATTR void ota_upgrade_start() {
	int step = STEP_UPGRADE_START;
	const char *msg = "";
	if (system_upgrade_start(server)) {

	} else {
		ota_deinit();
		step = STEP_UPGRADE_FAILED;
		msg = ERROR_UPGRADING;
	}
	ota_upgrade_response(step, msg);
}

ICACHE_FLASH_ATTR static void ota_dns_found_cb(const char *name, ip_addr_t *ipaddr, void *arg) {
	os_timer_disarm(&timer);
	if (ipaddr == NULL || ipaddr->addr == 0) {
		ota_deinit();
		ota_upgrade_response(STEP_DOWNLOAD_FAILED, ERROR_GETIP_FAILED);
		return;
	}
	LOGD(TAG, "ipaddr: " IPSTR "\n", IP2STR(ipaddr));
	os_memcpy(server->ip, &ipaddr->addr, 4);
	ota_upgrade_start();
}

ICACHE_FLASH_ATTR static void ota_dns_timeout_cb(void *arg) {
	ota_deinit();
	ota_upgrade_response(STEP_DOWNLOAD_FAILED, ERROR_TIMEOUT);
}

ICACHE_FLASH_ATTR static void ota_start_dns(const char *host) {
	espconn_gethostbyname(server->pespconn, host, &ipaddr, ota_dns_found_cb);
	os_timer_disarm(&timer);
	os_timer_setfn(&timer, ota_dns_timeout_cb, NULL);
	os_timer_arm(&timer, DNS_TIMEOUT_PERIOD, 0);
}

ICACHE_FLASH_ATTR static void ota_restart(void *arg) {
	system_upgrade_reboot();
}

ICACHE_FLASH_ATTR static void ota_check_cb(void *arg) {
	struct upgrade_server_info *server = (struct upgrade_server_info *) arg;
	if (server->upgrade_flag == 1) {
		ota_upgrade_response(STEP_UPGRADE_SUCCESS, "");
		LOGD(TAG, "device ota success...\n");
	} else {
		ota_upgrade_response(STEP_UPGRADE_FAILED, ERROR_UPGRADE_FAILED);
		LOGD(TAG, "device ota failed...\n");
	}
	ota_deinit();

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, ota_restart, NULL);
	os_timer_arm(&timer, RESTART_DELAY, 0);
}

ICACHE_FLASH_ATTR bool ota_get_info(const char *url, ota_info_t *pinfo) {
	if(url == NULL) {
		ota_upgrade_response(STEP_DOWNLOAD_FAILED, ERROR_INVALID_URL);
		return false;
	}
	int offset = 0;
	if (os_strncmp(url, "http://", os_strlen("http://")) == 0) {
		offset = os_strlen("http://");
	} else if (os_strncmp(url, "https://", os_strlen("https://")) == 0) {
		offset = os_strlen("https://");
	}
	char *p = os_strchr(url + offset, '/');
	if (p == NULL) {
		ota_upgrade_response(STEP_DOWNLOAD_FAILED, ERROR_INVALID_URL);
		return false;
	}
	
	if (sizeof(pinfo->path) - 1 < os_strlen(p) ||
		sizeof(pinfo->host) - 1 < p - url - offset) {
		ota_upgrade_response(STEP_DOWNLOAD_FAILED, ERROR_URL_TOO_LONG);
		return false;
	}
 
	os_memset(pinfo, 0, sizeof(ota_info_t));
	os_strcpy(pinfo->path, p);
	os_memcpy(pinfo->host, url + offset, p - url - offset);
	p = os_strchr(pinfo->host, ':');
	if (p == NULL) {
		pinfo->port = 80;
	} else {
		pinfo->port = atoi(p+1);
		*p = '\0';
	}
	LOGD(TAG, "%s:%d", pinfo->host, pinfo->port);
	return true;
}

ICACHE_FLASH_ATTR bool check_ip(const char *ip, uint8_t *ipaddr) {
	uint8_t dot = 0;
	bool first_num_got = false;
	char first_num;
	int16_t val = -1;
	char temp;

	while(*ip != '\0') {
		temp = *ip++;
		if (temp >= '0' && temp <= '9') {
			if (first_num_got == false) {
				first_num_got = true;
				first_num = temp;
			} else if (first_num == '0') {
				return false;
			}
			if (val < 0) {
				val = 0;
			}
			val = val * 10 + (temp - '0');
			if (val > 255) {
				return false;
			}
			ipaddr[dot] = val;
		} else if (temp == '.') {
			/* .前面必须为 数字且在 0 ~ 255 之间  */
			if (val < 0 || val > 255) {
				return false;
			}
			dot++;
			if (dot > 3) {
				return false;
			}
			val = -1;
			first_num_got = false;
		} else {
			return false;
		}
	}
	return dot == 3 && val >= 0 && val <= 255;
}

ICACHE_FLASH_ATTR void ota_init(const ota_info_t *pinfo) {
	server = os_zalloc(sizeof(struct upgrade_server_info));
	if (server == NULL) {
		return;
	}
	server->pespconn = os_zalloc(sizeof(struct espconn));
	if (server->pespconn == NULL) {
		os_free(server);
		return;
	}
	int len = os_strlen(HEADER_FMT) + os_strlen(pinfo->path) + os_strlen(pinfo->host) + 6;
	server->url = os_zalloc(len);
	if (server->url == NULL) {
		os_free(server->pespconn);
		os_free(server);
		return;
	}
	server->port = pinfo->port;
	server->check_times = UPGRADE_TIMEOUT;
	server->check_cb = ota_check_cb;
	os_sprintf(server->url, HEADER_FMT, pinfo->path, pinfo->host, pinfo->port);
	LOGD(TAG, "%s", server->url);
}

ICACHE_FLASH_ATTR void ota_regist_progress_cb(void (*callback)(const int step, const char *msg)) {
	ota_callback.progress_cb = callback;
}

ICACHE_FLASH_ATTR void ota_start(const char *target_version, const char *url) {
	if (processing) {
		return;
	}

	int i;
	uint16_t current_version;
	for (i = 0; i < os_strlen(target_version); i++) {
		if (target_version[i] < '0' || target_version[i] > '9' || i >= 5) {
			ota_upgrade_response(STEP_UPGRADE_FAILED, ERROR_INVALID_VERSION);
			return;
		}
	}
	uint16_t upgrade_version = atoi(target_version);
	current_version = user_device_get_version();
	if (current_version >= upgrade_version || ((upgrade_version-current_version)&0x01 == 0)) {
		ota_upgrade_response(STEP_UPGRADE_FAILED, ERROR_INVALID_VERSION);
		return;
	}

	ota_info_t ota_info;
	os_memset(&ota_info, 0 , sizeof(ota_info));
	if (ota_get_info(url, &ota_info)) {
		ota_init(&ota_info);

		LOGD(TAG, "%s:%d", ota_info.host, ota_info.port);
		uint8_t ipaddr[4];
		if (check_ip(ota_info.host, ipaddr)) {
			os_memcpy(server->ip, ipaddr, 4);
			ota_upgrade_start();
		} else {
			ota_start_dns(ota_info.host);
			processing = true;
		}
	}
}
