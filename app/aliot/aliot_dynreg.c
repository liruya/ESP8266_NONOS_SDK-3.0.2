// #include "aliot_dynreg.h"
// #include "aliot_sign.h"
// #include "aliot_wrappers.h"

// //	"iot-auth.${region}.aliyuncs.com"
// #define	DYNREG_HOSTNAME_FMT			"iot-auth.%s.aliyuncs.com"

// //	deviceName${deviceName}productKey${productKey}random${random}
// #define	DYNREG_SIGNSOURCE_FMT		"deviceName%sproductKey%srandom%s"	

// //	${region}	${data_len}
// #define DYNREG_REGISTER_FMT     	"POST /auth/register/device  HTTP/1.1\r\n\
// Host: iot-auth.%s.aliyuncs.com\r\n\
// Accept: text/xml,text/javascript,text/html,application/json\r\n\
// Content-Type: application/x-www-form-urlencoded\r\n\
// Content-Length: %d\r\n\r\n\
// %s"

// //	${productKey} ${deviceName} ${random} ${sign} ${signMethod}
// #define	DYNREG_REGDATA_FMT			"productKey=%s&deviceName=%s&random=%s&sign=%s&signMethod=%s"

// void hal_tcp_secure_connect(const char *host);
// void hal_tcp_secure_disconnect();
// void hal_tcp_secure_send(const char *data);
// void hal_tcp_regist_conn_cb(void (*conn_cb)());
// void hal_tcp_regist_recv_cb(void (*recv_cb)(const char *data, int len));
// void hal_set_device_secret(const char *dev_secret);
// void hal_random(char *random, int len);

// FUNC(int) aliot_dynreg_sign(const char *pkey, const char *psecret, const char *dname, const char *random, char *sign) {
// 	uint32_t sign_source_len = 0;
//     uint8_t  *sign_source = NULL;
// 	aliot_sign_t *psign = aliot_sign_get();

//     sign_source_len = strlen(DYNREG_SIGNSOURCE_FMT) + strlen(dname) + strlen(pkey) + strlen(random) + 1;
//     sign_source = hal_zalloc(sign_source_len);
//     if (sign_source == NULL) {
//         hal_printf("malloc sign_source failed.\n");
//         return -1;
//     }

//     hal_snprintf(sign_source, sign_source_len, DYNREG_SIGNSOURCE_FMT, dname, pkey, random);
//     psign->function(sign_source, strlen(sign_source), psecret, strlen(psecret), sign);
//     hal_free(sign_source);
//     hal_printf("sign: %s\n", sign);

// 	return 0;
// }

// FUNC(int) aliot_dynreg_send_request(const char *pkey, const char *psecret, const char *dname, const char *random) {
// 	int sign_source_len = 0;
//     char *sign_source = NULL;
// 	aliot_sign_t *psign = aliot_sign_get();

//     sign_source_len = strlen(DYNREG_SIGNSOURCE_FMT) + strlen(dname) + strlen(pkey) + strlen(random) + 1;
//     sign_source = hal_zalloc(sign_source_len);
//     if (sign_source == NULL) {
//         hal_printf("malloc sign_source failed.\n");
//         return -1;
//     }
//     hal_snprintf(sign_source, sign_source_len, DYNREG_SIGNSOURCE_FMT, dname, pkey, random);

// 	char signout[65];
// 	os_memset(signout, 0 , sizeof(signout));
//     psign->function(sign_source, strlen(sign_source), psecret, strlen(psecret), signout);
//     hal_free(sign_source);
//     hal_printf("sign: %s\n", signout);

// 	int datlen = strlen(DYNREG_REGDATA_FMT) + strlen(pkey) + strlen(dname) + strlen(random) + strlen(signout) + strlen(psign->name) + 1;
// 	char *data = hal_zalloc(datlen);
// 	if (data == NULL) {
// 		hal_printf("malloc data failed.\n");
// 		return;
// 	}
// 	hal_snprintf(data, datlen, DYNREG_REGDATA_FMT, pkey, dname, random, "", psign->name);

// 	int len = strlen(DYNREG_REGISTER_FMT) + strlen("region") + 8 + strlen(data);
// 	char *buf = hal_zalloc(len);
// 	if (buf == NULL) {
// 		hal_printf("malloc buf failed.\n");
// 		hal_free(data);
// 		return;
// 	}
// 	hal_snprintf(buf, len, DYNREG_REGISTER_FMT, "region", datlen, data);
// 	hal_printf("%s\n", buf);
// 	hal_tcp_secure_send(buf);

// 	hal_free(data);
// 	hal_free(buf);
// }

// FUNC(void) aliot_dynreg_send_register_string(const char *pkey, const char *psecret, const char *dname, const char *random) {
// 	aliot_sign_t *psign = aliot_sign_get();
// 	int datlen = strlen(DYNREG_REGDATA_FMT) + strlen(pkey) + strlen(psecret) + strlen(dname) + strlen(random) + strlen(psign->name) + 65;
// 	char *data = hal_zalloc(datlen);
// 	if (data == NULL) {
// 		hal_printf("malloc data failed.\n");
// 		return;
// 	}
// 	hal_snprintf(data, datlen, DYNREG_REGDATA_FMT, pkey, dname, random, "", psign->name);

// 	int len = strlen(DYNREG_REGISTER_FMT) + strlen("region") + 8 + strlen(data);
// 	char *buf = hal_zalloc(len);
// 	if (buf == NULL) {
// 		hal_printf("malloc buf failed.\n");
// 		hal_free(data);
// 		return;
// 	}
// 	hal_snprintf(buf, len, DYNREG_REGISTER_FMT, "region", datlen, data);
// 	hal_printf("%s\n", buf);
// 	hal_tcp_secure_send(buf);

// 	hal_free(data);
// 	hal_free(buf);
// }

// FUNC(void) aliot_dynreg_connect_cb() {
// 	aliot_dynreg_send_register_string();
// }

// FUNC(int) aliot_dynreg_parse_result(const char *payload) {
// 	char *pkCode = os_strstr(payload, "{\"code\":");
//     if (pkCode == NULL) {
//         hal_printf("dynreg failed.\n");
//         return -1;
//     }
//     char *pkCodeSuc = os_strstr(pkCode, "{\"code\":200");
//     char *pkData = os_strstr(pkCode, "\"data\":");
//     if (pkCodeSuc == NULL || pkData == NULL) {
//         hal_printf("dynreg failed. --> %s\n", pkCode);
//         return -1;
//     }
//     char *pkSecret = os_strstr(pkData, "\"deviceSecret\":");
//     char *pStart = os_strchr(pkSecret + strlen("\"deviceSecret\":"), '\"');
//     char *pEnd = os_strchr(pStart+1, '\"');
//     hal_printf("start: %d  %s\n", pStart, pStart);
//     hal_printf("end: %d  %s\n", pEnd, pEnd);
//     if (pkSecret == NULL || pStart == NULL || pEnd == NULL || pEnd - pStart > 65) {
//         hal_printf("dynreg failed. no deviceSecret.\n");
//         return -1;
//     }
//     char secret[65];
//     os_memset(secret, 0, sizeof(secret));
//     os_memcpy(secret, pStart+1, pEnd - pStart - 1);
//     hal_printf("deviceSecret: %s.\n", secret);
// 	hal_set_device_secret(secret);
// 	return 0;
// }

// FUNC(void) aliot_dynreg_start(aliot_dynreg_config_t *pcfg) {
// 	char sign[65];
// 	os_memset(sign, 0, sizeof(sign));
// 	aliot_dynreg_sign(pcfg->pkey, pcfg->psecret, pcfg->dname, pcfg->random, sign);

// 	hal_tcp_regist_conn_cb();
// 	hal_tcp_regist_recv_cb();
// 	hal_tcp_connect(host);
// }
