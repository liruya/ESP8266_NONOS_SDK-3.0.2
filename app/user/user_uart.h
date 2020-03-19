#ifndef	__USER_UART_H__
#define	__USER_UART_H__

#include "driver/uart.h"
#include "app_common.h"

#define	STOP_BITS_ONE		1
#define	STOP_BITS_ONE_HALF	2
#define	STOP_BITS_TWO		3

#define	DATA_BITS_FIVE		0
#define	DATA_BITS_SIX		1
#define	DATA_BITS_SEVEN		2
#define	DATA_BITS_EIGHT		3

#define	BAUDRATE_9600		9600
#define BAUDRATE_19200		19200
#define	BAUDRATE_38400		38400
#define	BAUDRATE_57600		57600
#define	BAUDRATE_74880		74880
#define	BAUDRATE_115200		115200

/* 00 10 11 bit1:enable */
#define	PARITY_NONE			0
#define	PARITY_EVEN			2
#define	PARITY_ODD			3

typedef void (* uart_rx_cb_t)(uint8_t *pbuf, uint8_t len);
typedef void (* uart_tx_empty_cb_t)(void);

extern void uart0_init(uint32_t rate, uint32_t frame_interval);
extern STATUS uart0_send_byte_nowait(uint8_t byte);
extern uint8_t uart0_send_byte(uint8_t byte);
extern void uart0_send_buffer(uint8_t *buf, uint32_t len);
extern void uart0_set_rx_cb(uart_rx_cb_t cb);
extern void uart0_set_tx_empty_cb(uart_tx_empty_cb_t cb);

extern void uart1_init(uint32_t rate);
extern STATUS uart1_send_byte_nowait(uint8_t byte);
extern uint8_t uart1_send_byte(uint8_t byte);
extern void uart1_send_buffer(uint8_t *buf, uint32_t len);
extern void uart1_set_tx_empty_cb(uart_tx_empty_cb_t cb);

extern void uart_enable_isr ();
extern void uart_disable_isr ();

#endif