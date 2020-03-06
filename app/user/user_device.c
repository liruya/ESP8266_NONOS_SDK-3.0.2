#include "user_device.h"
#include "aliot_mqtt.h"
#include "mem.h"

static const char *TAG = "Device";

#define	KEY_VALUE_FMT	"\"%s\":%s"

ICACHE_FLASH_ATTR int getNumberString(attr_t *attr, char *buf) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->toText == NULL) {
		return 0;
	}
	return os_sprintf(buf, KEY_NUMBER_FMT, attr->attrKey, *((int *) attr->attrValue));
}

ICACHE_FLASH_ATTR int getTextString(attr_t *attr, char *buf) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->toText == NULL) {
		return 0;
	}
	return os_sprintf(buf, KEY_TEXT_FMT, attr->attrKey, (char *) attr->attrValue);
}

#define DEVMODEL_PROPERTY_TOPIC_POST		"/sys/%s/%s/thing/event/property/post"
#define DM_POST_FMT "{\"id\":\"%d\",\"version\":\"1.0\",\"params\":{%s},\"method\":\"thing.event.property.post\"}"
void ICACHE_FLASH_ATTR user_device_post_property(attr_t *attr) {
	if (attr == NULL || attr->vtable == NULL || attr->vtable->toText == NULL) {
		return;
	}
	char params[1024];
	os_memset(params, 0, sizeof(params));
	attr->vtable->toText(attr, params);

    int len = os_strlen(DM_POST_FMT) + os_strlen(params) + 1;
    char *payload = os_zalloc(len);
    if (payload == NULL) {
        os_printf("malloc payload failed.\n");
        return;
    }
    os_snprintf(payload, len, DM_POST_FMT, aliot_mqtt_getid(), params);
    os_printf("%s\n", payload);
    aliot_mqtt_publish(DEVMODEL_PROPERTY_TOPIC_POST, payload, 0, 0);
}

// void ICACHE_FLASH_ATTR user_device_post_properties(property_t *prop, int cnt) {

// }

#define	PSW_ENABLE_MASK	0xFF000000
#define	PSW_ENABLE_FLAG	0x55000000
#define	PSW_MASK		0x00FFFFFF
#define	PSW_MAX			999999

// bool ICACHE_FLASH_ATTR user_device_psw_isvalid(user_device_t *pdev) {
// 	if (pdev == NULL || pdev->pconfig == NULL) {
// 		return false;
// 	}
// 	if ((pdev->pconfig->local_psw&PSW_ENABLE_MASK) == PSW_ENABLE_FLAG && (pdev->pconfig->local_psw&PSW_MASK) <= PSW_MAX) {
// 		return true;
// 	}
// 	return false;
// }

bool ICACHE_FLASH_ATTR user_device_poweron_check(user_device_t *pdev) {
	if (pdev == NULL) {
		return false;
	}
	bool flag = false;
	uint8_t cnt = 0;

	while(cnt < 250) {
		cnt++;
		os_delay_us(10000);
		system_soft_wdt_feed();
		if (GPIO_INPUT_GET(pdev->key_io_num) == 0) {
			flag = true;
			break;
		}
	}

	if (flag) {
		cnt = 0;
		//长按4秒
		while(cnt < 200) {
			system_soft_wdt_feed();
			os_delay_us(20000);
			if(GPIO_INPUT_GET(pdev->key_io_num) == 0) {
				cnt++;
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

void ICACHE_FLASH_ATTR user_device_process(void *arg) {
	user_device_t *pdev = arg;
	if (pdev == NULL) {
		return;
	}
	// sint8_t rssi = wifi_station_get_rssi();

	pdev->process(arg);
}

void ICACHE_FLASH_ATTR user_device_init(user_device_t *pdev) {
	if (pdev == NULL) {
		ERR(TAG, "device init failed...");
		return;
	}
	pdev->board_init();
	pdev->init();
	// if (user_device_poweron_check(pdev)) {
	// 	system_restore();
	// 	app_test_init(pdev);
	// } else {
	// 	pdev->init();
	// }
	// os_timer_disarm(&proc_timer);
	// os_timer_setfn(&proc_timer, user_device_process, pdev);
	// os_timer_arm(&proc_timer, 1000, 1);
}
