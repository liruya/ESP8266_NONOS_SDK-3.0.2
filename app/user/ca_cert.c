#include "ca_cert.h"
#include "osapi.h"
#include "user_interface.h"

#define	CA_CERT_NAME				"TLS.ca_x509.cer"

#define	CA_CERT_CONTENT				"-----BEGIN CERTIFICATE-----\n\
MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\n\
A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\n\
b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\n\
MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\n\
YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\n\
aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\n\
jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\n\
xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\n\
1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\n\
snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\n\
U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\n\
9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\n\
BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\n\
AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\n\
yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\n\
38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\n\
AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\n\
DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\n\
HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\n\
-----END CERTIFICATE-----\n"

//	4字节对齐
typedef struct {
	char name[32];
	char dummy[2];
	char content[1278];
} ca_cert_t;

ICACHE_FLASH_ATTR void check_ca_bin() {
	ca_cert_t ca;
	os_memset(&ca, 0, sizeof(ca));
	spi_flash_read(CA_SECTOR_ADDR*SPI_FLASH_SEC_SIZE, (uint32_t *) &ca, sizeof(ca));
	if (os_strcmp(ca.name, CA_CERT_NAME) != 0
		|| ca.dummy[0] != 0xED
		|| ca.dummy[1] != 0x04
		|| os_strcmp(ca.content, CA_CERT_CONTENT) != 0) {

		spi_flash_erase_sector(CA_SECTOR_ADDR);
		os_memset(&ca, 0, sizeof(ca));
		os_strcpy(ca.name, CA_CERT_NAME);
		ca.dummy[0] = 0xED;
		ca.dummy[1] = 0x04;
		os_strcpy(ca.content, CA_CERT_CONTENT);
		spi_flash_write(CA_SECTOR_ADDR*SPI_FLASH_SEC_SIZE, (uint32_t *) &ca, sizeof(ca));
	}
}
