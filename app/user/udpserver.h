#ifndef	_UDPSERVER_H_
#define	_UDPSERVER_H_

#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "app_common.h"

extern	bool udpserver_remote_valid();
extern	void udpserver_init(void (*callback)(const char *buf));
extern	uint32_t udpserver_send(uint8_t *pdata, uint16_t len);

#endif