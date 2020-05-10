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
#include "user_monsoon.h"

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

// static dev_meta_info_t meta;
static bool validDeviceSecret;
static user_device_t *pdev;

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
    const char* rst_msg[7] = {"Poweron", "HWDT", "Exception", "SWDT", "Soft Restart", "Sleep Awake", "Ext Reset"};
    struct rst_info *info = system_get_rst_info();
    uint32_t reason = info->reason;
    if (reason >= 0 && reason < 7) {
        LOGD(TAG, "Reset reason: %d  \t%s", reason, rst_msg[reason]);
    }
    if (reason == REASON_WDT_RST || reason == REASON_EXCEPTION_RST || reason == REASON_SOFT_WDT_RST) {
        if (reason == REASON_EXCEPTION_RST) {
            LOGD(TAG, "Fatal exception (%d): ", info->exccause);
        }
        LOGD(TAG, "epc1=0x%08x, epc2=0x%08x, epc3=0x%08x, excvaddr=0x%08x, depc=0x%08x", 
                    info->epc1, info->epc2, info->epc3, info->excvaddr, info->depc);
    }
}

ICACHE_FLASH_ATTR void  dynreg_success_cb(const char *dsecret) {
    if (hal_set_device_secret(dsecret) && hal_get_device_secret(pdev->meta.device_secret)) {
        validDeviceSecret = true;
        aliot_mqtt_init(&pdev->meta);
        aliot_mqtt_connect();
    }
}

ICACHE_FLASH_ATTR void  wifi_event_cb(System_Event_t *evt) {
    switch (evt->event) {
        case EVENT_STAMODE_CONNECTED:
            LOGD(TAG, "start to connect to ssid %s, channel %d",
                evt->event_info.connected.ssid,
                evt->event_info.connected.channel);
            if (pdev != NULL) {
                os_memset(pdev->dev_info.ssid, 0, sizeof(pdev->dev_info.ssid));
                os_strcpy(pdev->dev_info.ssid, evt->event_info.connected.ssid);
            }
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
            if (pdev != NULL) {
                os_memset(pdev->dev_info.ipaddr, 0, sizeof(pdev->dev_info.ipaddr));
                os_sprintf(pdev->dev_info.ipaddr, IPSTR, IP2STR(&evt->event_info.got_ip.ip));
            }
            if (validDeviceSecret) {
                aliot_mqtt_connect();
            } else {
                dynreg_start(&pdev->meta, dynreg_success_cb);
                // aliot_mqtt_dynregist(&meta);
            }
            break;
        default:
            break;
    }
}

ICACHE_FLASH_ATTR void wifi_connect(const char* ssid, const char* pass) {
	struct station_config config;

	wifi_set_opmode_current(STATION_MODE);
    wifi_station_disconnect();

	os_memset(&config, 0, sizeof(config));
	os_snprintf(config.ssid, sizeof(config.ssid), "%s", ssid);
	os_snprintf(config.password, sizeof(config.password), "%s", pass);
    config.threshold.authmode = AUTH_WPA_WPA2_PSK;

	wifi_station_set_config_current(&config);
	wifi_station_connect();
}

ICACHE_FLASH_ATTR void product_init() {
    if (pdev == NULL) {
        return;
    }
    os_memset(&pdev->meta, 0, sizeof(pdev->meta));
    
    hal_get_region(pdev->meta.region);
    hal_get_product_key(pdev->meta.product_key);
    hal_get_product_secret(pdev->meta.product_secret);
    
    pdev->meta.firmware_version = pdev->firmware_version;
    if (os_strcmp(pdev->region, pdev->meta.region) != 0) {
        hal_set_region(pdev->region);
        os_memset(pdev->meta.region, 0, sizeof(pdev->meta.region));
        os_strcpy(pdev->meta.region, pdev->region);
    }
    if (os_strcmp(pdev->productKey, pdev->meta.product_key) != 0) {
        hal_set_product_key(pdev->productKey);
        os_memset(pdev->meta.product_key, 0, sizeof(pdev->meta.product_key));
        os_strcpy(pdev->meta.product_key, pdev->productKey);
    }
    if (os_strcmp(pdev->productSecret, pdev->meta.product_secret) != 0) {
        hal_set_product_secret(pdev->productSecret);
        os_memset(pdev->meta.product_secret, 0, sizeof(pdev->meta.product_secret));
        os_strcpy(pdev->meta.product_secret, pdev->productSecret);
    }
    
    hal_get_device_name(pdev->meta.device_name);
    if (hal_get_device_secret(pdev->meta.device_secret)) {
        validDeviceSecret = true;
        aliot_mqtt_init(&pdev->meta);
    }
    aliot_attr_init(&pdev->meta);
    LOGD(TAG, "fw_version: %d", pdev->meta.firmware_version);
    LOGD(TAG, "region: %s", pdev->meta.region);
    LOGD(TAG, "productKey: %s", pdev->meta.product_key);
    LOGD(TAG, "productSecret: %s", pdev->meta.product_secret);
    LOGD(TAG, "deviceName: %s", pdev->meta.device_name);
    LOGD(TAG, "deviceSecret: %s", pdev->meta.device_secret);
}

void user_init(void) {
#if   (PRODUCT_TYPE == PRODUCT_TYPE_LED)
    pdev = &user_dev_led;
#elif   (PRODUCT_TYPE == PRODUCT_TYPE_SOCKET)
    pdev = &user_dev_socket;
#elif   (PRODUCT_TYPE == PRODUCT_TYPE_MONSOON)
    pdev = &user_dev_monsoon;
#else
#error "Invalid PRODUCT_TYPE.(0-ExoLed, 1-ExoSocket, 2-ExoMonsoon)"
#endif
    // system_set_os_print(false);
    user_device_attch_instance(pdev);

    user_device_board_init();
    app_print_reset_cause();
    os_delay_us(60000);

    user_device_init();
    product_init();

    ota_regist_progress_cb(aliot_mqtt_report_fota_progress);
    aliot_regist_fota_upgrade_cb(ota_start);
    aliot_regist_sntp_response_cb(user_rtc_set_time);
    aliot_regist_connect_cb(user_rtc_sync_time);

    wifi_set_event_handler_cb(wifi_event_cb);
    // wifi_connect(SSID, PASSWORD);

    LOGI(TAG, "System started ...");
}
