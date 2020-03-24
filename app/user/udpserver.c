#include "udpserver.h"

#define	UDP_LOCAL_PORT			8266

typedef struct {
	void (* parse_rcv)(const char *buf);
} udpserver_callback_t;

static const char *TAG = "UDP";

struct espconn udpserver;
udpserver_callback_t udpserver_cb;

ICACHE_FLASH_ATTR uint32_t udpserver_send(uint8_t *pdata, uint16_t len) {
	espconn_send(&udpserver, pdata, len);
	return len;
}

ICACHE_FLASH_ATTR static void udpserver_recv_cb(void *arg, char *pdata, unsigned short len) {
	if(arg == NULL || pdata == NULL || len == 0) {
		return;
	}
	struct espconn *pespconn = (struct espconn *) arg;
	remot_info remote;
	remot_info *premote = &remote;
	if(espconn_get_connection_info(pespconn, &premote, 0) != ESPCONN_OK) {
		return;
	}
	LOGD(TAG, "remote ip: " IPSTR "  port: %d", IP2STR(premote->remote_ip), premote->remote_port);
	os_memcpy(udpserver.proto.udp->remote_ip, premote->remote_ip, 4);
	udpserver.proto.udp->remote_port = premote->remote_port;

	char *buf = os_zalloc(len+1);
	if (buf == NULL) {
		return;
	}
	os_memcpy(buf, pdata, len);
	if (udpserver_cb.parse_rcv != NULL) {
		udpserver_cb.parse_rcv(buf);
	}
}

ICACHE_FLASH_ATTR void udpserver_init(void (*callback)(const char *buf)) {
	udpserver_cb.parse_rcv = callback;

	udpserver.type = ESPCONN_UDP;
	udpserver.state = ESPCONN_NONE;
	udpserver.proto.udp = (esp_udp *) os_zalloc(sizeof(esp_udp));
	if (udpserver.proto.udp == NULL) {
		LOGD(TAG, "malloc udp failed.");
		return;
	}
	udpserver.reverse = NULL;
	udpserver.proto.udp->local_port = UDP_LOCAL_PORT;
	udpserver.proto.udp->remote_ip[0] = 0x00;
	udpserver.proto.udp->remote_ip[1] = 0x00;
	udpserver.proto.udp->remote_ip[2] = 0x00;
	udpserver.proto.udp->remote_ip[3] = 0x00;
	espconn_regist_recvcb(&udpserver, udpserver_recv_cb);
	espconn_create(&udpserver);
}
