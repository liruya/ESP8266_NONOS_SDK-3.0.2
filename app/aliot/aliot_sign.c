#include "aliot_sign.h"
#include "aliot_types.h"
#include "infra_md5.h"
#include "infra_sha1.h"
#include "infra_sha256.h"					

static aliot_sign_type_t sign_type = HMACSHA256;

static const aliot_sign_t signs[3] = {
	{
		.name = "hmacmd5",
		.function = utils_hmac_md5
	},
	{
		.name = "hmacsha1",
		.function = utils_hmac_sha1
	},
	{
		.name = "hmacsha256",
		.function = utils_hmac_sha256
	}
};

FUNC(void) aliot_sign_set(aliot_sign_type_t type) {
	sign_type = type;
}

FUNC(aliot_sign_t*) aliot_sign_get() {
	switch (sign_type) {
		case HMACMD5:
			return (aliot_sign_t*) &signs[0];
		case HMACSHA1:
			return (aliot_sign_t*) &signs[1];
		case HMACSHA256:
			return (aliot_sign_t*) &signs[2];
		default:
			break;
	}
	return (aliot_sign_t*) &signs[2];
}