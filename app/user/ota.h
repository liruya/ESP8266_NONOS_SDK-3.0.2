#ifndef	_OTA_H_
#define	_OTA_H_

#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "upgrade.h"

extern	void ota_regist_progress_cb(void (*callback)(const int step, const char *msg));
extern	void ota_start(const char *target_version, const char *url);

#endif