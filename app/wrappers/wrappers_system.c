#include "wrappers_system.h"

void ICACHE_FLASH_ATTR *hal_malloc(size_t size) {
	return os_malloc(size);
}

void ICACHE_FLASH_ATTR hal_free(void *ptr) {
	os_free(ptr);
}

void ICACHE_FLASH_ATTR *hal_zalloc(size_t size) {
	return os_zalloc(size);
}

void ICACHE_FLASH_ATTR hal_memset(void *dest, int val, unsigned int nbyte) {
	os_memset(dest, val, nbyte);
}

void ICACHE_FLASH_ATTR hal_memcpy(void *dest, const void *src, unsigned int nbyte) {
	os_memcpy(dest, src, nbyte);
}





