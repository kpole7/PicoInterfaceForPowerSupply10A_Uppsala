// serial_transmission.c

#include <stdbool.h>	// just for Eclipse

#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "serial_transmission.h"

#include <stdio.h>		// just for debugging

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define UART_ID				uart0
#define UART_BAUD_RATE		4800
#define UART_TX_PIN 		0
#define UART_RX_PIN 		1
#define UART_IRQ			UART0_IRQ
#define UART_DATA_BITS		8
#define UART_PARITY			UART_PARITY_NONE

#define UART_BUFFER_SIZE					32
#define SILENCE_DETECTION_IN_MICROSECONDS	6250		// 3 bytes for the given baud rate
#define LONGEST_COMMAND_LENGTH				12			// ???
#define REPLACEMENT_FOR_UNPRINTABLE			'~'


//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

static char UartInputBuffer[UART_BUFFER_SIZE];
static uint8_t UartInputHead, UartInputTail;
static uint64_t WhenReceivedLastByte;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is an interrupt handler for receiving and transmitting via UART
static void serialPortInterruptHandler( void );


//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void serialPortInitialization(void){
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

    UartInputHead = 0;
	UartInputTail = 0;
	WhenReceivedLastByte = (uint64_t)0;
}

void serialPortReceiver(void){
	if (UartInputHead != UartInputTail){
		// The input buffer is not empty

		uint64_t Now = time_us_64();
		if (WhenReceivedLastByte + SILENCE_DETECTION_IN_MICROSECONDS < Now){
			// silence detected in receiver

			int16_t ReceivedBytes;
			ReceivedBytes = UartInputHead - UartInputTail;
			if (ReceivedBytes < 0){
				ReceivedBytes += UART_BUFFER_SIZE;
			}
			if (ReceivedBytes <= LONGEST_COMMAND_LENGTH){
				// The number of received bytes is reasonable; they should be interpreted

#if 1
				// for debugging purpose
				printf("Received %d bytes [", ReceivedBytes );
				while(UartInputHead != UartInputTail){
					if ((UartInputBuffer[UartInputTail] >= ' ') && (UartInputBuffer[UartInputTail] <= (char)127)){
						printf("%c", UartInputBuffer[UartInputTail] );
					}
					else{
						printf("%c", REPLACEMENT_FOR_UNPRINTABLE );
					}
					UartInputTail++;
		   	   		if (UartInputTail >= UART_BUFFER_SIZE){
		   	   			UartInputTail = 0;
		   	   		}
				}
				printf("]\n");
#endif




			}
		}
	}



}

static void serialPortInterruptHandler( void ){
   	while(uart_is_readable(UART_ID)){
   		UartInputBuffer[UartInputHead] = uart_getc(UART_ID);
   		UartInputHead++;
   		if (UartInputHead >= UART_BUFFER_SIZE){
   			UartInputHead = 0;
   		}
   		if (UartInputHead == UartInputTail){
   			UartInputTail++;
   	   		if (UartInputTail >= UART_BUFFER_SIZE){
   	   			UartInputTail = 0;
   	   		}
   		}
   		WhenReceivedLastByte = time_us_64();
   	}
}




void serialPortTransmitter(void){
//	if(uart_is_writable( UART_ID )){
//		uart_putc_raw( UART_ID, temporary ); // uart_putc is not good due to its CRLF support
//	}
}


