/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "mqtt.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

#include "cJSON.h"
#include "dev_sign.h"
#include "aliot_sign.h"

#include "dynreg.h"
#include "wrappers_product.h"
#include "aliot_mqtt.h"
#include "ota.h"

#include "user_rtc.h"
#include "user_device.h"
#include "user_led.h"
#include "user_socket.h"

#if ((SPI_FLASH_SIZE_MAP == 0) || (SPI_FLASH_SIZE_MAP == 1))
#error "The flash map is not supported"
#elif (SPI_FLASH_SIZE_MAP == 2)
#define SYSTEM_PARTITION_OTA_SIZE							0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR							0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR						0xfb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR						0xfc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR				0xfd000
#elif (SPI_FLASH_SIZE_MAP == 3)
#define SYSTEM_PARTITION_OTA_SIZE							0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR							0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR						0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR						0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR				0x1fd000
#elif (SPI_FLASH_SIZE_MAP == 4)
#define SYSTEM_PARTITION_OTA_SIZE							0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR							0x81000
#define SYSTEM_PARTITION_RF_CAL_ADDR						0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR						0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR				0x3fd000
#elif (SPI_FLASH_SIZE_MAP == 5)
#define SYSTEM_PARTITION_OTA_SIZE							0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR							0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR						0x1fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR						0x1fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR				0x1fd000
#elif (SPI_FLASH_SIZE_MAP == 6)
#define SYSTEM_PARTITION_OTA_SIZE							0x6A000
#define SYSTEM_PARTITION_OTA_2_ADDR							0x101000
#define SYSTEM_PARTITION_RF_CAL_ADDR						0x3fb000
#define SYSTEM_PARTITION_PHY_DATA_ADDR						0x3fc000
#define SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR				0x3fd000
#else
#error "The flash map is not supported"
#endif

// #define SSID        "GalaxyS9"
// #define PASSWORD    "fkfu4678"
#define SSID        "TP-LINK_F370"
#define PASSWORD    "inledco370"
// #define SSID        "ASUS-RT-AC68U"
// #define PASSWORD    "asdfqwer"

static const char *TAG = "Main";

static dev_meta_info_t meta;
static bool validDeviceSecret;

static const partition_item_t at_partition_table[] = {
    { SYSTEM_PARTITION_BOOTLOADER, 						0x0, 												0x1000},
    { SYSTEM_PARTITION_OTA_1,   						0x1000, 											SYSTEM_PARTITION_OTA_SIZE},
    { SYSTEM_PARTITION_OTA_2,   						SYSTEM_PARTITION_OTA_2_ADDR, 						SYSTEM_PARTITION_OTA_SIZE},
    { SYSTEM_PARTITION_RF_CAL,  						SYSTEM_PARTITION_RF_CAL_ADDR, 						0x1000},
    { SYSTEM_PARTITION_PHY_DATA, 						SYSTEM_PARTITION_PHY_DATA_ADDR, 					0x1000},
    { SYSTEM_PARTITION_SYSTEM_PARAMETER, 				SYSTEM_PARTITION_SYSTEM_PARAMETER_ADDR, 			0x3000},
};

ICACHE_FLASH_ATTR void user_pre_init(void) {
    if(!system_partition_table_regist(at_partition_table, sizeof(at_partition_table)/sizeof(at_partition_table[0]), SPI_FLASH_SIZE_MAP)) {
		LOGD(TAG, "system_partition_table_regist fail.");
		while(1);
	}
}

ICACHE_FLASH_ATTR void  app_print_reset_cause() {
    struct rst_info *info = system_get_rst_info();
    uint32_t reason = info->reason;
    LOGD(TAG, "Reset reason: %x", info->reason);
    if (reason == REASON_WDT_RST || reason == REASON_EXCEPTION_RST || reason == REASON_SOFT_WDT_RST) {
        if (reason == REASON_EXCEPTION_RST) {
            LOGD(TAG, "Fatal exception (%d): ", info->exccause);
        }
        LOGD(TAG, "epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x", 
                    info->epc1, info->epc2, info->epc3, info->excvaddr, info->depc);
    }
}

ICACHE_FLASH_ATTR void  dynreg_success_cb(const char *dsecret) {
    if (hal_set_device_secret(dsecret) && hal_get_device_secret(meta.device_secret)) {
        validDeviceSecret = true;
        aliot_mqtt_init(&meta);
        aliot_mqtt_connect();
    }
}

ICACHE_FLASH_ATTR void  wifi_event_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED:
            LOGD(TAG, "start to connect to ssid %s, channel %d",
                evt->event_info.connected.ssid,
                evt->event_info.connected.channel);
            break;
        case EVENT_STAMODE_DISCONNECTED:
            LOGD(TAG, "disconnect from ssid %s, reason %d",
                evt->event_info.disconnected.ssid,
                evt->event_info.disconnected.reason);
                aliot_mqtt_disconnect();
            break;
        case EVENT_STAMODE_AUTHMODE_CHANGE:
            LOGD(TAG, "mode: %d -> %d",
                evt->event_info.auth_change.old_mode,
                evt->event_info.auth_change.new_mode);
            break;
        case EVENT_STAMODE_GOT_IP:
            LOGD(TAG, "ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR "",
                IP2STR(&evt->event_info.got_ip.ip),
                IP2STR(&evt->event_info.got_ip.mask),
                IP2STR(&evt->event_info.got_ip.gw));
            if (validDeviceSecret) {
                aliot_mqtt_connect();
            } else {
                dynreg_start(&meta, dynreg_success_cb);
            }
            break;
        default:
        break;
    }
}

ICACHE_FLASH_ATTR void  wifi_connect(char* ssid, char* pass) {
	struct station_config config;

	wifi_set_opmode_current(STATION_MODE);

	os_memset(&config, 0, sizeof(struct station_config));

	os_snprintf(config.ssid, sizeof(config.ssid), "%s", ssid);
	os_snprintf(config.password, sizeof(config.password), "%s", pass);

	wifi_station_set_config_current(&config);

	wifi_station_connect();
}

ICACHE_FLASH_ATTR void product_init() {
    os_memset(&meta, 0, sizeof(meta));
    hal_get_region(meta.region);
    hal_get_product_key(meta.product_key);
    hal_get_product_secret(meta.product_secret);
    hal_get_device_name(meta.device_name);
    hal_get_version(&meta.firmware_version);
    if (hal_get_device_secret(meta.device_secret)) {
        validDeviceSecret = true;
        aliot_mqtt_init(&meta);
    }
    LOGD(TAG, "region: %s", meta.region);
    LOGD(TAG, "productKey: %s", meta.product_key);
    LOGD(TAG, "productSecret: %s", meta.product_secret);
    LOGD(TAG, "deviceName: %s", meta.device_name);
    LOGD(TAG, "deviceSecret: %s", meta.device_secret);
}

ICACHE_FLASH_ATTR void  float2string(float val, char *str) {
    int a = val;
    float b = val - a;
    if (a < 0) {
        b = -b;
    }
    if (b == 0) {
        os_sprintf(str, "%d", a);
        return;
    }
    int i;
    int c = 0;
    int d = 0;
    for (i = 0; i < 5; i++) {
        b *= 10;
        c = b;
        d = d*10 + c;
        b -= c;
        if (b <= 0) {
            break;
        }
    }
    os_sprintf(str, "%d.%d", a, d);
}

void user_init(void) {
    user_device_board_init(&user_dev_socket);
    app_print_reset_cause();
    os_delay_us(60000);

    user_device_init(&user_dev_socket);
    product_init();

    ota_regist_progress_cb(aliot_mqtt_report_fota_progress);
    aliot_regist_fota_upgrade_cb(ota_start);
    aliot_regist_sntp_response_cb(user_rtc_set_time);
    aliot_regist_connect_cb(user_rtc_sync_time);

    wifi_set_event_handler_cb(wifi_event_cb);
    wifi_connect(SSID, PASSWORD);

    LOGI(TAG, "System started ...");
}
