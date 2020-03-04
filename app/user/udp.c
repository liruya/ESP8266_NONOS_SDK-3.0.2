#include "udp.h"

#define	UDP_LOCAL_PORT			8266
#define	UDP_REMOTE_PORT			10086

struct espconn udpserver;

LOCAL void ICACHE_FLASH_ATTR udp_recv_cb(void *arg, char *pdata, unsigned short len) {
	if(arg == NULL || pdata == NULL || len == 0) {
		return;
	}
	struct espconn *pespconn = (struct espconn *) arg;
	remot_info remote;
	remot_info *premote = &remote;
	if(espconn_get_connection_info(pespconn, &premote, 0) != ESPCONN_OK) {
		return;
	}
	os_printf("remote ip: " IPSTR "  port: %d\n", IP2STR(pespconn->proto.udp->remote_ip), pespconn->proto.udp->remote_port);
	os_printf("remote ip: " IPSTR "  port: %d\n", IP2STR(premote->remote_ip), premote->remote_port);
	os_memcpy(udpserver.proto.udp->remote_ip, premote->remote_ip, 4);
	udpserver.proto.udp->remote_port = premote->remote_port;	
}

uint32_t ICACHE_FLASH_ATTR udp_send(struct espconn *pespconn, uint8_t *pdata, uint16_t len) {
	espconn_send(pespconn, pdata, len);
	return len;
}

void ICACHE_FLASH_ATTR udp_init() {
	udpserver.type = ESPCONN_UDP;
	udpserver.state = ESPCONN_NONE;
	udpserver.proto.udp = (esp_udp *) os_zalloc(sizeof(esp_udp));
	if (udpserver.proto.udp == NULL) {

		os_printf("malloc udp failed.\n");
		while (1);
	}
	udpserver.reverse = NULL;
	udpserver.proto.udp->local_port = UDP_LOCAL_PORT;
	udpserver.proto.udp->remote_port = UDP_REMOTE_PORT;
	udpserver.proto.udp->remote_ip[0] = 0x00;
	udpserver.proto.udp->remote_ip[1] = 0x00;
	udpserver.proto.udp->remote_ip[2] = 0x00;
	udpserver.proto.udp->remote_ip[3] = 0x00;
	espconn_regist_recvcb(&udpserver, udp_recv_cb);
	espconn_create(&udpserver);
}
