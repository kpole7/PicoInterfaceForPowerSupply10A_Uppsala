// serial_transmission.c

#include <stdbool.h>	// just for Eclipse

#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "serial_transmission.h"
#include "debugging.h"

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
static volatile uint8_t UartInputHead, UartInputTail;
static volatile uint64_t WhenReceivedLastByte;

static char UartOutputBuffer[UART_BUFFER_SIZE];
static volatile uint8_t UartOutputHead, UartOutputTail;

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
				// The number of received bytes is reasonable; they can be called 'new frame'

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
	changeDebugPin2(true);

	if (uart_is_readable(UART_ID)){
	   	do{
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
	   	}while(uart_is_readable(UART_ID));
	}
	else{
	   	if (UartOutputHead <= UART_BUFFER_SIZE){ // protection just in case
	   	   	if((UartOutputTail < UartOutputHead) && uart_is_writable( UART_ID )){
	   	   		uart_putc_raw( UART_ID, UartOutputBuffer[UartOutputTail] ); // uart_putc is not good due to its CRLF support
	   	   		UartOutputTail++;
	   	   	}
	   	   	if (UartOutputTail >= UartOutputHead){
	   	   		uart_set_irq_enables( UART_ID, true, false );	// there is nothing more to send so stop interrupts from the sender
	   	   	}
	   	}
	   	else{
	   		while(1){
	   			; // intentionally hang the program
	   		}
	   	}
	}

	changeDebugPin2(false);
}

int8_t transmitViaSerialPort(void){
	if (0 == UartOutputHead){
		return -1; // The buffer is empty; it is nothing to do
	}
	if (UartOutputTail != 0){
		return -1; // printout is going on (it should never happen)
	}

	if(!uart_is_writable( UART_ID )){
		return -1; // conflict with the previous printout (it should never happen)
	}
	if (UartOutputHead > UART_BUFFER_SIZE){
		return -1; // improper value of UartOutputHead (it should never happen)
	}

	// write data to UART
	uart_putc_raw( UART_ID, UartOutputBuffer[UartOutputTail] ); // uart_putc is not good due to its CRLF support
	UartOutputTail++;

	if (UartOutputTail < UartOutputHead){
		uart_set_irq_enables( UART_ID, true, true );	// the next bytes will be sent in the interrupt handler
	}

	return 0;
}

void testSending(void){
	//						    12345678901234567890123456789012
	sprintf( UartOutputBuffer, "Abcdefghijklmnopqrstuvwxyz12345" );
	UartOutputBuffer[31] = '.';
	UartOutputHead = 32;
	UartOutputTail = 0;

	transmitViaSerialPort();
}

