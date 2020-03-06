#ifndef	__APP_COMMON_H__
#define	__APP_COMMON_H__

#include "user_interface.h"
#include "osapi.h"
#include "mem.h"
#include "gpio.h"
#include "driver/gpio16.h"
#include "driver/uart.h"
#include "espconn.h"

#define	USE_TX_DEBUG

#define	LOG_DISABLED	0
#define	LOG_LEVEL_E		1			//error
#define	LOG_LEVEL_W		2			//warning
#define	LOG_LEVEL_I		3			//info
#define	LOG_LEVEL_D		4			//debug
#define	LOG_LEVEL_V		5			//verbose

#define	LOG_LEVEL		LOG_LEVEL_D

#define	VRB(TAG, format, ...)		
#define	DBG(TAG, format, ...)			
#define	INF(TAG, format, ...)			
#define	WRN(TAG, format, ...)			
#define	ERR(TAG, format, ...)			

#if		(defined(LOG_LEVEL)) && (LOG_LEVEL != LOG_DISABLED)
//verbose
#if		LOG_LEVEL >= LOG_LEVEL_V
#undef	VRB
#define	VRB(TAG, format, ...)		os_printf("\n[VERBOSE] %s @%4d: " format "\n", TAG, __LINE__, ##__VA_ARGS__)
#endif
//debug
#if		LOG_LEVEL >= LOG_LEVEL_D
#undef	DBG
#define	DBG(TAG, format, ...)			os_printf("\n[DEBUG] %s @%4d: " format "\n", TAG, __LINE__, ##__VA_ARGS__)
#endif
//info
#if		LOG_LEVEL >= LOG_LEVEL_I
#undef	INF
#define	INF(TAG, format, ...)			os_printf("\n[INFO] %s @%4d: " format "\n", TAG, __LINE__, ##__VA_ARGS__)
#endif
//warn
#if		LOG_LEVEL >= LOG_LEVEL_W
#undef	WRN
#define	WRN(TAG, format, ...)			os_printf("\n[WARN] %s @%4d: " format "\n", TAG, __LINE__, ##__VA_ARGS__)
#endif
//error
#if		LOG_LEVEL >= LOG_LEVEL_E
#undef	ERR
#define	ERR(TAG, format, ...)			os_printf("\n[ERROR] %s @%4d: " format "\n", TAG, __LINE__, ##__VA_ARGS__)
#endif

#endif

#define	ESPFUNC					ICACHE_FLASH_ATTR

#define	GPIO_OUTPUT_GET(pin)	((GPIO_REG_READ(GPIO_OUT_ADDRESS)>>(pin))&BIT0)
#define	GPIO16_OUTPUT_GET()		(READ_PERI_REG(RTC_GPIO_OUT)&(uint32)0x00000001)

#define	gpio_high(pin)			do {\
									if (pin == 16) {\
										gpio16_output_set(1);\
									} else {\
										GPIO_OUTPUT_SET(pin, 1);\
									}\
								} while (0)
#define	gpio_low(pin)			do {\
									if (pin == 16) {\
										gpio16_output_set(0);\
									} else {\
										GPIO_OUTPUT_SET(pin, 0);\
									}\
								} while (0)
#define	gpio_toggle(pin)		do {\
									if (pin == 16) {\
										gpio16_output_set(!GPIO16_OUTPUT_GET());\
									} else {\
										GPIO_OUTPUT_SET(pin, !GPIO_OUTPUT_GET(pin));\
									}\
								} while (0)

#endif