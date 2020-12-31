#ifndef	_OTA_H_
#define	_OTA_H_

#include "osapi.h"
#include "user_interface.h"
#include "ets_sys.h"
#include "mem.h"
#include "upgrade.h"
#include "user_code.h"
#include "cJSON.h"

extern	void 	ota_regist_progress_cb(void (*callback)(const int8_t step, const char *msg));
extern	char* 	ota_start(const cJSON *params);

#endif