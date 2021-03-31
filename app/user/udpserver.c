#include "udpserver.h"

#define	UDP_LOCAL_PORT			8899

typedef struct {
	void (* parse_rcv)(const char *buf);
} udpserver_callback_t;

static const char *TAG = "UDPServer";

static esp_udp udp;
static struct espconn udpserver;
udpserver_callback_t udpserver_cb;

ICACHE_FLASH_ATTR uint32_t udpserver_send(uint8_t *pdata, uint16_t len) {
	espconn_send(&udpserver, pdata, len);
	return len;
}

ICACHE_FLASH_ATTR static void udpserver_sent_cb(void *arg) {
	
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

	os_memset(&udp, 0, sizeof(udp));
	udp.local_port = UDP_LOCAL_PORT;

	os_memset(&udpserver, 0, sizeof(udpserver));
	udpserver.state = ESPCONN_NONE;
	udpserver.type = ESPCONN_UDP;
	udpserver.reverse = NULL;
	udpserver.proto.udp = &udp;
	espconn_regist_recvcb(&udpserver, udpserver_recv_cb);
	espconn_regist_sentcb(&udpserver, udpserver_sent_cb);
	espconn_create(&udpserver);
}
