#include "app_board_socket.h"
#include "driver/uart.h"
#include "user_uart.h"

ICACHE_FLASH_ATTR void app_board_socket_init() {
	gpio_init();
	uart1_init(BAUDRATE_74880);
	UART_SetPrintPort(UART1);
	system_uart_swap();

	GPIO_DIS_OUTPUT(KEY_IO_NUM);
	GPIO_DIS_OUTPUT(DETECT_IO_NUM);

	PIN_FUNC_SELECT(KEY_IO_MUX, KEY_IO_FUNC);
	PIN_FUNC_SELECT(DETECT_IO_MUX, DETECT_IO_FUNC);
	PIN_FUNC_SELECT(LEDR_IO_MUX, LEDR_IO_FUNC);
	PIN_FUNC_SELECT(LEDG_IO_MUX, LEDG_IO_FUNC);
	PIN_FUNC_SELECT(LEDB_IO_MUX, LEDB_IO_FUNC);
	PIN_FUNC_SELECT(RELAY1_IO_MUX, RELAY1_IO_FUNC);
	ledr_off();
	ledg_off();
	ledb_off();
	relay_off();
}
