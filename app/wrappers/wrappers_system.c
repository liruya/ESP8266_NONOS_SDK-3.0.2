#include "wrappers_system.h"

ICACHE_FLASH_ATTR void *hal_malloc(size_t size) {
	return os_malloc(size);
}

ICACHE_FLASH_ATTR void hal_free(void *ptr) {
	os_free(ptr);
}

ICACHE_FLASH_ATTR void *hal_zalloc(size_t size) {
	return os_zalloc(size);
}

ICACHE_FLASH_ATTR void hal_memset(void *dest, int val, unsigned int nbyte) {
	os_memset(dest, val, nbyte);
}

ICACHE_FLASH_ATTR void hal_memcpy(void *dest, const void *src, unsigned int nbyte) {
	os_memcpy(dest, src, nbyte);
}





