// serial_transmission.c

#include <stdbool.h>	// just for Eclipse

#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/regs/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "serial_transmission.h"
#include "rstl_protocol.h"
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

#define UART_INPUT_BUFFER_SIZE				32
#define UART_OUTPUT_BUFFER_SIZE				200
#define SILENCE_DETECTION_IN_MICROSECONDS	6250		// 3 bytes for the given baud rate
#define REPLACEMENT_FOR_UNPRINTABLE			'~'

#define UART_WARNING_INCOMING_WHILE_OUTGOING	0x01

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

static char UartInputBuffer[UART_INPUT_BUFFER_SIZE];
static volatile uint8_t UartInputHead, UartInputTail;
static volatile uint64_t WhenReceivedLastByte;

static char UartOutputBuffer[UART_OUTPUT_BUFFER_SIZE];
static volatile uint8_t UartOutputHead, UartOutputTail;
static volatile uint8_t UartError;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

static bool is_tx_irq_enabled(uart_inst_t *uart);

/// @brief This is an interrupt handler for receiving and transmitting via UART
static void serialPortInterruptHandler( void );

static inline void increaseModulo( volatile uint8_t * ArgumentPtr, const uint8_t Divisor );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

// Function that checks whether TX IRQ is enabled
static bool is_tx_irq_enabled(uart_inst_t *uart) {
	return (uart_get_hw(uart)->imsc & UART_UARTIMSC_TXIM_BITS) != 0;
}

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
	UartError = 0;
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
				ReceivedBytes += UART_INPUT_BUFFER_SIZE;
			}
			if (ReceivedBytes <= LONGEST_COMMAND_LENGTH){
				// The number of received bytes is reasonable; they can be treated as new command

				int16_t Index = 0;
				while(UartInputHead != UartInputTail){
					NewCommand[Index] = UartInputBuffer[UartInputTail];
					Index++;
					if (Index >= COMMAND_BUFFER_LENGTH-1){
						break;
					}
					increaseModulo( &UartInputTail, UART_INPUT_BUFFER_SIZE);
				}
				NewCommand[Index] = 0;

				executeCommand();
			}
		}
	}
}

static void serialPortInterruptHandler( void ){
	if (uart_is_readable(UART_ID)){
		char IncomingCharacter = uart_getc(UART_ID);
		UartInputBuffer[UartInputHead] = IncomingCharacter;
		increaseModulo( &UartInputHead, UART_INPUT_BUFFER_SIZE);
		if (UartInputHead == UartInputTail){
			// the buffer is overflowing; the oldest data is overwritten
			increaseModulo( &UartInputTail, UART_INPUT_BUFFER_SIZE);
		}
		WhenReceivedLastByte = time_us_64(); // to check how long the silence lasts in the incoming transmission
		// Check if there is any outgoing transmission
		if (is_tx_irq_enabled(UART_ID) || !uart_is_writable( UART_ID ) || (UartOutputHead != UartOutputTail)){
			// cancel outgoing buffered transmission
   	   		uart_set_irq_enables( UART_ID, true, false );
   	   		UartOutputHead = 0;
   	   		UartOutputTail = 0;
   	   		UartError |= UART_WARNING_INCOMING_WHILE_OUTGOING;
		}
		if (uart_is_writable( UART_ID )){
			uart_putc_raw( UART_ID, IncomingCharacter ); // send echo
		}
	}
	else{
	   	if ((UartOutputHead < UART_OUTPUT_BUFFER_SIZE) &&
	   			(UartOutputTail < UART_OUTPUT_BUFFER_SIZE)) // protection just in case
	   	{
	   	   	if((UartOutputTail != UartOutputHead) && uart_is_writable( UART_ID )){
	   	   		uart_putc_raw( UART_ID, UartOutputBuffer[UartOutputTail] ); // uart_putc is not good due to its CRLF support
	   	   		increaseModulo( &UartOutputTail, UART_OUTPUT_BUFFER_SIZE);
	   	   	}
	   	   	if (UartOutputTail == UartOutputHead){
	   	   		uart_set_irq_enables( UART_ID, true, false );	// there is nothing more to send so stop interrupts from the sender
	   	   	}
	   	}
	   	else{
	   		while(1){
	   			; // intentionally hang the program
	   		}
	   	}
	}
}

int8_t transmitViaSerialPort( const char* TextToBeSent ){
	if (NULL == TextToBeSent){
		return -1; // improper value of the argument
	}
	if (0 == TextToBeSent[0]){
		return -1; // incorrect value pointed to by argument
	}

	if (UartOutputHead >= UART_OUTPUT_BUFFER_SIZE){
		return -1; // improper value of UartOutputHead (it should never happen)
	}
	if (UartOutputTail >= UART_OUTPUT_BUFFER_SIZE){
		return -1; // improper value of UartOutputTail (it should never happen)
	}

	if (UartOutputHead+1 == UartOutputTail){
		return -1; // The output buffer is full
	}
	if ((UART_OUTPUT_BUFFER_SIZE-1 == UartOutputHead) && (0 == UartOutputTail)){
		return -1; // The output buffer is full
	}

	if (is_tx_irq_enabled(UART_ID) || !uart_is_writable( UART_ID )){
		// TX UART interrupt is active (the UART is transmitting now)
    	return -1;	// writing to the buffer during transmission should not occur
	}
	// TX UART interrupt is not active (the UART is not transmitting now)

	uint8_t Index = 0;
	uint8_t FutureHeadValue = UartOutputHead;
	increaseModulo( &FutureHeadValue, UART_OUTPUT_BUFFER_SIZE);
	while ((FutureHeadValue != UartOutputTail) && (0 != TextToBeSent[Index])){
		UartOutputBuffer[UartOutputHead] = TextToBeSent[Index];
		Index++;
		UartOutputHead = FutureHeadValue;
		increaseModulo( &FutureHeadValue, UART_OUTPUT_BUFFER_SIZE);
	}
	if (uart_is_writable( UART_ID )){ // the condition should always be met
		// write data to UART
		uart_putc_raw( UART_ID, UartOutputBuffer[UartOutputTail] ); // uart_putc is not good due to its CRLF support
		increaseModulo( &UartOutputTail, UART_OUTPUT_BUFFER_SIZE);
#if 0
		printf(">>> putc >>> %d  %d  %d\n", (int)UartOutputHead, (int)UartOutputTail, (int)Index );
#endif
	}
	uart_set_irq_enables( UART_ID, true, true );	// the next bytes will be sent in the interrupt handler

	if (FutureHeadValue == UartOutputHead){
		return -1; // The output buffer is full
	}

	return 0;
}

static inline void increaseModulo( volatile uint8_t * ArgumentPtr, const uint8_t Divisor ){
	ArgumentPtr[0]++;
	if (ArgumentPtr[0] >= Divisor){
		ArgumentPtr[0] = 0;
	}
}

