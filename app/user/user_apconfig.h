#ifndef	__USER_APCONFIG_H__
#define	__USER_APCONFIG_H__

#include "app_common.h"
#include "user_task.h"

extern void user_apconfig_instance_start(const task_impl_t *, const uint32_t, const char *, void (* const done)(int16_t));
extern void user_apconfig_instance_stop();
extern bool user_apconfig_instance_status();

#endif