#include "ali_httpc.h"
#include "osapi.h"
#include "user_interface.h"
#include "espconn.h"

#define	DNS_RETRY_INTERVAL_MIN		2
#define	DNS_RETRY_INTERVAL_MAX		64

struct espconn *pclient;
static os_timer_t timer;
ip_addr_t remoteip;

void ICACHE_FLASH_ATTR http_dns_found(const char *name, ip_addr_t *ip, void *arg) {
	os_timer_disarm(&timer);
    struct espconn *pespconn = (struct espconn *) arg;
	if(ip == NULL || ip->addr == 0) {
		return;
	}
    os_printf("dns found: " IPSTR "\n", IP2STR(ip));
	os_memcpy(pespconn->proto.tcp->remote_ip, &ip->addr, 4);
	espconn_connect(pespconn);
}

void ICACHE_FLASH_ATTR http_check_dns(void *arg) {
	struct espconn *pespconn = (struct espconn *) arg;
	remoteip.addr = 0;
	// espconn_gethostbyname(pespconn, domain, &remoteip, http_dns_found);
}

void ICACHE_FLASH_ATTR http_start_dns(struct espconn *pespconn, const char *domain) {
	remoteip.addr = 0;
    espconn_gethostbyname(pespconn, domain, &remoteip, http_dns_found);

	os_timer_disarm(&timer);
	os_timer_setfn(&timer, http_check_dns, pespconn);
	os_timer_arm(&timer, 2000, 1);
    os_printf("start dns...\n");
}

void ICACHE_FLASH_ATTR http_disconnect(struct espconn *pespconn) {
	espconn_disconnect(pespconn);
}
