#ifndef	__APP_BOARD_SOCKET_H__
#define	__APP_BOARD_SOCKET_H__

#include "app_common.h"

#define PERIPHS_IO_MUX_GPIO1_U		PERIPHS_IO_MUX_U0TXD_U
#define PERIPHS_IO_MUX_GPIO3_U		PERIPHS_IO_MUX_U0RXD_U
#define PERIPHS_IO_MUX_GPIO12_U		PERIPHS_IO_MUX_MTDI_U
#define PERIPHS_IO_MUX_GPIO13_U		PERIPHS_IO_MUX_MTCK_U
#define PERIPHS_IO_MUX_GPIO14_U		PERIPHS_IO_MUX_MTMS_U
#define PERIPHS_IO_MUX_GPIO15_U		PERIPHS_IO_MUX_MTDO_U

#define BOOT_IO_NUM					0
#define BOOT_IO_MUX					PERIPHS_IO_MUX_GPIO0_U
#define BOOT_IO_FUNC				FUNC_GPIO0

#define LEDR_IO_NUM					0
#define LEDR_IO_MUX					PERIPHS_IO_MUX_GPIO0_U
#define LEDR_IO_FUNC				FUNC_GPIO0
#define LEDG_IO_NUM					12
#define LEDG_IO_MUX					PERIPHS_IO_MUX_GPIO12_U
#define LEDG_IO_FUNC				FUNC_GPIO12
#define LEDB_IO_NUM					1
#define LEDB_IO_MUX					PERIPHS_IO_MUX_GPIO1_U
#define LEDB_IO_FUNC				FUNC_GPIO1
#define DETECT_IO_NUM				5
#define DETECT_IO_MUX				PERIPHS_IO_MUX_GPIO5_U
#define DETECT_IO_FUNC				FUNC_GPIO5
#define RELAY1_IO_NUM				4
#define RELAY1_IO_MUX				PERIPHS_IO_MUX_GPIO4_U
#define RELAY1_IO_FUNC				FUNC_GPIO4
#define KEY_IO_NUM					14
#define KEY_IO_MUX					PERIPHS_IO_MUX_GPIO14_U
#define KEY_IO_FUNC					FUNC_GPIO14

#define ledr_on()           GPIO_OUTPUT_SET(LEDR_IO_NUM, 0)
#define ledr_off()          GPIO_OUTPUT_SET(LEDR_IO_NUM, 1)
#define ledr_toggle()       GPIO_OUTPUT_SET(LEDR_IO_NUM, GPIO_OUTPUT_GET(LEDR_IO_NUM) == 0 ? 1 : 0)
#define ledg_on()           GPIO_OUTPUT_SET(LEDG_IO_NUM, 0)
#define ledg_off()          GPIO_OUTPUT_SET(LEDG_IO_NUM, 1)
#define ledg_toggle()       GPIO_OUTPUT_SET(LEDG_IO_NUM, GPIO_OUTPUT_GET(LEDG_IO_NUM) == 0 ? 1 : 0)
#define ledb_on()           GPIO_OUTPUT_SET(LEDB_IO_NUM, 0)
#define ledb_off()          GPIO_OUTPUT_SET(LEDB_IO_NUM, 1)
#define ledb_toggle()       GPIO_OUTPUT_SET(LEDB_IO_NUM, GPIO_OUTPUT_GET(LEDB_IO_NUM) == 0 ? 1 : 0)
#define relay_on()          GPIO_OUTPUT_SET(RELAY1_IO_NUM, 1)
#define relay_off()         GPIO_OUTPUT_SET(RELAY1_IO_NUM, 0)
#define	relay_status()		GPIO_OUTPUT_GET(RELAY1_IO_NUM)

#define	ledall_on()			ledr_on();\
							ledg_on();\
							ledb_on()
#define	ledall_off()		ledr_off();\
							ledg_off();\
							ledb_off()
#define	ledall_toggle()		ledr_toggle();\
							ledg_toggle();\
							ledb_toggle();

extern void app_board_socket_init();

#endif