#ifndef	_OTA_H_
#define	_OTA_H_

#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "upgrade.h"

typedef	void (*response_cb_t)(const int step, const char *msg);

extern	void ota_start(const char *target_version, const char *url, response_cb_t response_cb);

#endif