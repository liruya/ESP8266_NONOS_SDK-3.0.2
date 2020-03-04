#ifndef	_DYNREG_H_
#define	_DYNREG_H_

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"
#include "aliot_defs.h"

typedef	void (* dynreg_success_cb_t)(const char *);

extern	void dynreg_start(dev_meta_info_t *meta, dynreg_success_cb_t success_cb);

#endif