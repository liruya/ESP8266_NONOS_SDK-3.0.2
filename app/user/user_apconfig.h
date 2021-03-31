#ifndef	__USER_APCONFIG_H__
#define	__USER_APCONFIG_H__

#include "app_common.h"

extern bool user_apconfig_start(const char *apssid, const uint32_t timeout, void (* pre_cb)(), void (* post_cb), void (*set_time)(int zone, uint64_t time));
extern void user_apconfig_stop();
extern bool user_apconfig_status();

#endif