#ifndef	__APP_BOARD_LED_H__
#define	__APP_BOARD_LED_H__

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

#define	PWM1_IO_NUM					14
#define PWM1_IO_MUX					PERIPHS_IO_MUX_GPIO14_U
#define PWM1_IO_FUNC				FUNC_GPIO14
#define	PWM2_IO_NUM					12
#define PWM2_IO_MUX					PERIPHS_IO_MUX_GPIO12_U
#define PWM2_IO_FUNC				FUNC_GPIO12
#define	PWM3_IO_NUM					13
#define PWM3_IO_MUX					PERIPHS_IO_MUX_GPIO13_U
#define PWM3_IO_FUNC				FUNC_GPIO13
#define	PWM4_IO_NUM					15
#define PWM4_IO_MUX					PERIPHS_IO_MUX_GPIO15_U
#define PWM4_IO_FUNC				FUNC_GPIO15
#define	PWM5_IO_NUM					5
#define PWM5_IO_MUX					PERIPHS_IO_MUX_GPIO5_U
#define PWM5_IO_FUNC				FUNC_GPIO5
#define TOUCH_IO_NUM				4
#define TOUCH_IO_MUX				PERIPHS_IO_MUX_GPIO4_U
#define TOUCH_IO_FUNC				FUNC_GPIO4
#define LEDR_IO_NUM					2
#define LEDR_IO_MUX					PERIPHS_IO_MUX_GPIO2_U
#define LEDR_IO_FUNC				FUNC_GPIO2
#define LEDG_IO_NUM					0
#define LEDG_IO_MUX					PERIPHS_IO_MUX_GPIO0_U
#define LEDG_IO_FUNC				FUNC_GPIO0
#define LEDB_IO_NUM					16
// #define LEDB_IO_MUX			PERIPHS_IO_MUX_GPIO16_U
// #define LEDB_IO_FUNC			FUNC_GPIO16

#define ledr_on()           GPIO_OUTPUT_SET(LEDR_IO_NUM, 0)
#define ledr_off()          GPIO_OUTPUT_SET(LEDR_IO_NUM, 1)
#define ledr_toggle()       GPIO_OUTPUT_SET(LEDR_IO_NUM, GPIO_OUTPUT_GET(LEDR_IO_NUM) == 0 ? 1 : 0)
#define ledg_on()           GPIO_OUTPUT_SET(LEDG_IO_NUM, 0)
#define ledg_off()          GPIO_OUTPUT_SET(LEDG_IO_NUM, 1)
#define ledg_toggle()       GPIO_OUTPUT_SET(LEDG_IO_NUM, GPIO_OUTPUT_GET(LEDG_IO_NUM) == 0 ? 1 : 0)
#define ledb_on()           gpio16_output_set(0)
#define ledb_off()          gpio16_output_set(1)
#define ledb_toggle()       gpio16_output_set(GPIO16_OUTPUT_GET() == 0 ? 1 : 0)

#define	ledall_on()			ledr_on();\
							ledg_on();\
							ledb_on()
#define	ledall_off()		ledr_off();\
							ledg_off();\
							ledb_off()
#define	ledall_toggle()		ledr_toggle();\
							ledg_toggle();\
							ledb_toggle();

extern void app_board_led_init();

#endif