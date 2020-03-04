#ifndef	_DEV_SIGN_H_
#define	_DEV_SIGN_H_

#include "aliot_types.h"

#define DEV_SIGN_SOURCE_MAXLEN    	(200)
#define DEV_SIGN_HOSTNAME_MAXLEN  	(64)
#define DEV_SIGN_CLIENTID_MAXLEN	(200)
#define DEV_SIGN_USERNAME_MAXLEN  	(64)
#define DEV_SIGN_PASSWORD_MAXLEN  	(65)

typedef struct {
    char hostname[DEV_SIGN_HOSTNAME_MAXLEN];
    uint16_t port;
    char clientid[DEV_SIGN_CLIENTID_MAXLEN];
    char username[DEV_SIGN_USERNAME_MAXLEN];
    char password[DEV_SIGN_PASSWORD_MAXLEN];
} dev_sign_mqtt_t;

extern	int ali_mqtt_sign(const char *pkey, const char *devname, const char * devsecret, const char *region, dev_sign_mqtt_t *signout);

#endif