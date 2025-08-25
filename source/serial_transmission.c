
#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "serial_transmission.h"

#define UART_ID			uart0
#define UART_BAUD_RATE	4800
#define UART_TX_PIN 	0
#define UART_RX_PIN 	1
#define UART_IRQ		UART0_IRQ
#define UART_DATA_BITS	8
#define UART_PARITY		UART_PARITY_NONE
#define UART_BUFFER_MAX_SIZE	20



static char UartInputBuffer[UART_BUFFER_MAX_SIZE+2];
static char UartInputIndex;
static uint8_t UartInputHead;



static void serialPortInterruptHandler( void );



void serialPortInitialization(void){
	UartInputIndex = 0;

    uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));
    uart_set_baudrate( UART_ID, UART_BAUD_RATE );
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format( UART_ID, UART_DATA_BITS, 1, UART_PARITY );
    uart_set_fifo_enabled(UART_ID, false);

	irq_set_exclusive_handler(UART_IRQ, serialPortInterruptHandler);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables( UART_ID, true, false );


}

void serialPortReceiver(void){
	char temporary;

	while(uart_is_readable(UART_ID)){
		(void)uart_getc(UART_ID);
	}
	temporary = uart_getc( UART_ID ); //K.O.


	if(uart_is_writable( UART_ID )){
		uart_putc_raw( UART_ID, temporary ); // uart_putc is not good due to its CRLF support

	}
}


static void serialPortInterruptHandler( void ){
   	while(uart_is_readable(UART_ID)){
   		(void)uart_getc(UART_ID);
   	}
}






