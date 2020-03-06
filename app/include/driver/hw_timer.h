#ifndef	__HW_TIMER_H__
#define	__HW_TIMER_H__

#include "osapi.h"

typedef enum {
    FRC1_SOURCE = 0,
    NMI_SOURCE = 1,
} FRC1_TIMER_SOURCE_TYPE;

extern void hw_timer_arm(u32 val);
extern void hw_timer_set_func(void (* user_hw_timer_cb_set)(void));
extern void hw_timer_init(FRC1_TIMER_SOURCE_TYPE source_type, u8 req);
extern void hw_timer_disarm();
extern bool hw_timer_is_enabled();

#endif

/**
 * user add
 **/ 