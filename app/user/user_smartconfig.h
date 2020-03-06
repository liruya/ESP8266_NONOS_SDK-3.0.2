#ifndef	__USER_SMART_CONFIG_H__
#define	__USER_SMART_CONFIG_H__

#include "app_common.h"
#include "smartconfig.h"
#include "user_task.h"

extern void user_smartconfig_instance_start(const task_impl_t *, const uint32_t);
extern void user_smartconfig_instance_stop();
extern bool user_smartconfig_instance_status();

#endif