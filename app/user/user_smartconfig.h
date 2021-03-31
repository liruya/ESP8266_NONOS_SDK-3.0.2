#ifndef	__USER_SMART_CONFIG_H__
#define	__USER_SMART_CONFIG_H__

#include "app_common.h"
#include "smartconfig.h"

extern	bool user_smartconfig_start(const uint32_t timeout, void (* pre_cb)(), void (* post_cb)());
extern	void user_smartconfig_stop();
extern	bool user_smartconfig_status();

#endif