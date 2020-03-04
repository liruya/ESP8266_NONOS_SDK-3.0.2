#ifndef	_ALIOT_SIGN_H_
#define	_ALIOT_SIGN_H_

#include "aliot_defs.h"

typedef enum {
	HMACMD5,
	HMACSHA1,
	HMACSHA256
} aliot_sign_type_t;

typedef struct {
	const char *name;
	void (*function)(const char *msg, int msg_len, const char *key, int key_len, char *output);
} aliot_sign_t;

extern	void aliot_sign_set(aliot_sign_type_t type);

/**
 * default sign type is hmacsha256
 * */
extern	aliot_sign_t* aliot_sign_get();

#endif