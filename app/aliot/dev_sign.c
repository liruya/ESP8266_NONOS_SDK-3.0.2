#include "dev_sign.h"
#include "aliot_wrappers.h"
#include "aliot_defs.h"
#include "aliot_sign.h"
#include <string.h>

/* all secure mode define */
#define MODE_TLS_GUIDER             "-1"
#define MODE_TLS_DIRECT             "2"
#define MODE_TCP_DIRECT_PLAIN       "3"
#define MODE_ITLS_DNS_ID2           "8"

#if defined(SUPPORT_TLS)
    #define SECURE_MODE             MODE_TLS_DIRECT
#else
    #define SECURE_MODE             MODE_TCP_DIRECT_PLAIN
#endif

/* use fixed timestamp */
#define TIMESTAMP_VALUE             "2524608000000"

//  ${productKey}.${deviceName}
#define DEVICEID_FMT                "%s.%s"

//  ${deviceId}|timestamp=${timestamp},securemode=${securemode},signmethod=${signmethod}|
//  timestamp部分可省略
#define CLIENTID_FMT                "%s|timestamp=%s,securemode=%s,signmethod=%s|"

//  clientId${deviceId}deviceName%${deviceName}productKey${productKey}timestamp%${timestamp}"
#define SIGNSOURCE_FMT              "clientId%sdeviceName%sproductKey%stimestamp%s"

//  ${productKey}.iot-as-mqtt.${host}.aliyuncs.com
#define HOSTNAME_FMT                "%s.iot-as-mqtt.%s.aliyuncs.com"

//  ${deviceName}&${productKey}
#define USERNAME_FMT                "%s&%s"

static FUNC(void) hex2str(uint8_t *input, uint16_t input_len, char *output) {
    char *zEncode = "0123456789ABCDEF";
    int i = 0, j = 0;

    for (i = 0; i < input_len; i++) {
        output[j++] = zEncode[(input[i] >> 4) & 0xf];
        output[j++] = zEncode[(input[i]) & 0xf];
    }
}

FUNC(int) ali_mqtt_sign(const char *pkey, const char *devname, const char *devsecret, const char *region, dev_sign_mqtt_t *signout) {
	char clientid[PRODUCT_KEY_LEN + DEVICE_NAME_LEN + 1];

	if (strlen(pkey) + strlen(devname) + 1 > sizeof(clientid)) {
		return -1;
	}

	// clientId
	hal_memset(clientid, 0, sizeof(clientid));
    hal_snprintf(clientid, sizeof(clientid), DEVICEID_FMT, pkey, devname);

	hal_memset(signout, 0, sizeof(signout));
    aliot_sign_t *psign = aliot_sign_get();
	
	// clientid
    hal_snprintf(signout->clientid, sizeof(signout->clientid), CLIENTID_FMT, clientid, TIMESTAMP_VALUE, SECURE_MODE, psign->name);

    // password
    char signsource[DEV_SIGN_SOURCE_MAXLEN];
    hal_memset(signsource, 0, sizeof(signsource));
    hal_snprintf(signsource, sizeof(signsource), SIGNSOURCE_FMT, clientid, devname, pkey, TIMESTAMP_VALUE);
    
    psign->function(signsource, strlen(signsource), devsecret, strlen(devsecret), signout->password);

    // hostname
    hal_snprintf(signout->hostname, sizeof(signout->hostname), HOSTNAME_FMT, pkey, region);

    // username
    hal_snprintf(signout->username, sizeof(signout->username), USERNAME_FMT, devname, pkey);

	// port
#ifdef SUPPORT_TLS
    signout->port = 443;
#else
    signout->port = 1883;
#endif /* #ifdef SUPPORT_TLS */
    return 0;
}