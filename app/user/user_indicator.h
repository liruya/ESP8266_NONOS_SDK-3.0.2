#ifndef	__USER_INDICATOR_H__
#define	__USER_INDICATOR_H__

#include "app_common.h"

extern void user_indicator_start(const uint32_t period, const uint32_t count, void (* toggle)());
extern void user_indicator_stop();

#endif