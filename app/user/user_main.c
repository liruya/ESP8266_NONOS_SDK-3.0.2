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

#include "app_common.h"
#include "user_rtc.h"

#include "user_device.h"
#if     defined(PRODUCT_TYPE)
#if     PRODUCT_TYPE == PRODUCT_LED
#include "user_led.h"
static user_device_t *pdev  = &user_dev_led;
#elif   PRODUCT_TYPE == PRODUCT_SOCKET
#include "user_socket.h"
static user_device_t *pdev  = &user_dev_socket;
#elif   PRODUCT_TYPE == PRODUCT_MONSOON
#include "user_monsoon.h"
static user_device_t *pdev  = &user_dev_monsoon;
#else
#error  "Invalid PRODUCT_TYPE! (0-ExoLed, 1-ExoSocket, 2-ExoMonsoon)"
#endif
#else
#error "NO PRODUCT_TYPE defined"
#endif


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
#define SSID        "TP-LINK_56FC"
#define PASSWORD    "inledco56"

static const char *TAG = "Main";

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
		LOGD(TAG, "system_partition_table_regist fail. spi_flash_size_map: %d", SPI_FLASH_SIZE_MAP);
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

void user_init(void) {
    // system_set_os_print(false);
    LOGI(TAG, "compile time: %s", COMPILE_TIME);
    LOGI(TAG, "git commit id: %s", GIT_COMMITID);

    os_delay_us(60000);

    user_device_init(pdev);
    app_print_reset_cause();

    // wifi_connect(SSID, PASSWORD);

    LOGI(TAG, "System started ...");
}
