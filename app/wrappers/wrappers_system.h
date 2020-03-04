#ifndef	_WRAPPERS_SYSTEM_H_
#define	_WRAPPERS_SYSTEM_H_

#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
#include "mem.h"

#define	hal_printf		os_printf
#define	hal_sprintf		os_sprintf
#define	hal_snprintf	os_snprintf

extern	void *hal_malloc(size_t size);
extern	void hal_free(void *ptr);
extern	void *hal_zalloc(size_t size);
extern	void hal_memset(void *dest, int val, unsigned int nbyte);
extern	void hal_memcpy(void *dest, const void *src, unsigned int nbyte);

#endif