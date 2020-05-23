#include "udpserver.h"

#define	TIMEOUT					150000
#define	UDP_LOCAL_PORT			8899

typedef struct {
	void (* parse_rcv)(const char *buf);
} udpserver_callback_t;

static const char *TAG = "UDPServer";

static os_timer_t conn_tmr;
static bool valid;
static struct espconn udpserver;
udpserver_callback_t udpserver_cb;

ICACHE_FLASH_ATTR bool udpserver_remote_valid() {
	return valid;
}

ICACHE_FLASH_ATTR static void udpserver_check_cb(void *arg) {
	valid = false;
}

ICACHE_FLASH_ATTR static void udpserver_check() {
	valid = true;
	os_timer_disarm(&conn_tmr);
	os_timer_setfn(&conn_tmr, udpserver_check_cb, NULL);
	os_timer_arm(&conn_tmr, TIMEOUT, 0);
}

ICACHE_FLASH_ATTR uint32_t udpserver_send(uint8_t *pdata, uint16_t len) {
	LOGD(TAG, "len: %d", len);
	if (valid) {
		espconn_send(&udpserver, pdata, len);
		return len;
	}
	return 0;
}

ICACHE_FLASH_ATTR static void udpserver_sent_cb(void *arg) {
	LOGD(TAG, "udpserver sent");
}

ICACHE_FLASH_ATTR static void udpserver_recv_cb(void *arg, char *pdata, unsigned short len) {
	if(arg == NULL || pdata == NULL || len == 0) {
		return;
	}
	struct espconn *pespconn = (struct espconn *) arg;
	remot_info remote;
	remot_info *premote = &remote;
	if(espconn_get_connection_info(&udpserver, &premote, 0) != ESPCONN_OK) {
		return;
	}
	LOGD(TAG, "remote ip: " IPSTR "  port: %d", IP2STR(premote->remote_ip), premote->remote_port);
	os_memcpy(udpserver.proto.udp->remote_ip, premote->remote_ip, 4);
	udpserver.proto.udp->remote_port = premote->remote_port;
	udpserver_check();

	char *buf = os_zalloc(len+1);
	if (buf == NULL) {
		return;
	}
	os_memcpy(buf, pdata, len);
	if (udpserver_cb.parse_rcv != NULL) {
		udpserver_cb.parse_rcv(buf);
	}
	os_free(buf);
	buf = NULL;
}

ICACHE_FLASH_ATTR void udpserver_init(void (*callback)(const char *buf)) {
	udpserver_cb.parse_rcv = callback;

	udpserver.type = ESPCONN_UDP;
	udpserver.state = ESPCONN_NONE;
	udpserver.proto.udp = (esp_udp *) os_zalloc(sizeof(esp_udp));
	if (udpserver.proto.udp == NULL) {
		LOGE(TAG, "malloc udp failed.");
		return;
	}
	udpserver.reverse = NULL;
	udpserver.proto.udp->local_port = UDP_LOCAL_PORT;
	udpserver.proto.udp->remote_ip[0] = 0x00;
	udpserver.proto.udp->remote_ip[1] = 0x00;
	udpserver.proto.udp->remote_ip[2] = 0x00;
	udpserver.proto.udp->remote_ip[3] = 0x00;
	espconn_regist_recvcb(&udpserver, udpserver_recv_cb);
	espconn_regist_sentcb(&udpserver, udpserver_sent_cb);
	espconn_create(&udpserver);
}
