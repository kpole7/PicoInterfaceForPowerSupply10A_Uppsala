/// @file uart_talks.c

#include <stdbool.h>	// just for Eclipse

#include "hardware/timer.h"
#include "hardware/uart.h"
#include "hardware/regs/uart.h"
#include "hardware/irq.h"
#include "hardware/gpio.h"

#include "uart_talks.h"
#include "rstl_protocol.h"
#include "ring_spsc.h"
#include "debugging.h"

#include <stdio.h>		// just for debugging
#include <assert.h>

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define UART_ID				uart0
#define UART_BAUD_RATE		4800
#define GPIO_FOR_UART_TX 	0
#define GPIO_FOR_UART_RX 	1
#define UART_IRQ			UART0_IRQ
#define UART_DATA_BITS		8
#define UART_PARITY			UART_PARITY_NONE

#define UART_INPUT_BUFFER_SIZE				32			// buffer size (must be power-of-two)
#define UART_OUTPUT_BUFFER_SIZE				128			// buffer size (must be power-of-two)
#define SILENCE_DETECTION_IN_MICROSECONDS	6250		// 3 bytes for the given baud rate
#define REPLACEMENT_FOR_UNPRINTABLE			'~'

#define UART_ERROR_INPUT_BUFFER_OVERFLOW		0x01
#define UART_WARNING_INCOMING_WHILE_OUTGOING	0x02

static_assert( LONGEST_RESPONSE_LENGTH < UART_OUTPUT_BUFFER_SIZE, "static_assert LONGEST_RESPONSE_LENGTH < UART_OUTPUT_BUFFER_SIZE" );

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used in UART interrupt handler
atomic_uint_fast16_t UartError;

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used in UART interrupt handler
static char UartInputBuffer[UART_INPUT_BUFFER_SIZE];

static ring_spsc_t InputRingBuffer;

/// @brief This variable is used in UART interrupt handler
static atomic_uint_fast64_t WhenReceivedLastByte;

/// @brief This variable is used in UART interrupt handler
static char UartOutputBuffer[UART_OUTPUT_BUFFER_SIZE];

static ring_spsc_t OutputRingBuffer;

//---------------------------------------------------------------------------------------------------
// Local function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is an interrupt handler (callback function) for receiving and transmitting via UART
/// @callgraph
/// @callergraph
static void serialPortInterruptHandler( void );

/// Function that checks whether TX IRQ is enabled
static inline bool is_tx_irq_enabled(uart_inst_t *uart);

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes hardware port (UART) and initializes state machines for serial communication
void serialPortInitialization(void){

	ringSpscInit( &InputRingBuffer, (uint8_t*)UartInputBuffer, UART_INPUT_BUFFER_SIZE );
	ringSpscInit( &OutputRingBuffer, (uint8_t*)UartOutputBuffer, UART_OUTPUT_BUFFER_SIZE );

	atomic_store_explicit( &UartError, 0, memory_order_relaxed );
	atomic_store_explicit( &WhenReceivedLastByte, 0, memory_order_relaxed );

	uart_init(UART_ID, UART_BAUD_RATE);
    gpio_set_function(GPIO_FOR_UART_TX, UART_FUNCSEL_NUM(UART_ID, GPIO_FOR_UART_TX));
    gpio_set_function(GPIO_FOR_UART_RX, UART_FUNCSEL_NUM(UART_ID, GPIO_FOR_UART_RX));
    uart_set_baudrate( UART_ID, UART_BAUD_RATE );
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format( UART_ID, UART_DATA_BITS, 1, UART_PARITY );
    uart_set_fifo_enabled(UART_ID, false);

	irq_set_exclusive_handler(UART_IRQ, serialPortInterruptHandler);
    irq_set_enabled(UART_IRQ, true);
    uart_set_irq_enables( UART_ID, true, false );
}

/// @brief This function drives the state machine that receives frames via serial port
/// @return true if a new command has been received correctly via UART
/// @return false if there is no new command or there is an incorrect command
bool serialPortReceiver(void){
	bool Result = false;

	if (!ringSpscIsEmpty( &InputRingBuffer )){
		// The input buffer is not empty

		uint64_t Now = time_us_64();
		if (atomic_load_explicit( &WhenReceivedLastByte, memory_order_relaxed ) + SILENCE_DETECTION_IN_MICROSECONDS < Now){
			// silence detected in receiver

			bool ReceivedData = true;
			int16_t Index = 0;
			Result = true;
			while (!ringSpscIsEmpty( &InputRingBuffer )){
				ReceivedData = ringSpscPop( &InputRingBuffer, (uint8_t*)&NewCommand[Index] );
				if (!ReceivedData){
					break; // should never happen
				}
				Index++;
				if (Index > LONGEST_COMMAND_LENGTH){
					Result = false;
					break;
				}
				if (Index >= COMMAND_BUFFER_LENGTH-1){
					Result = false;
					break;
				}
			}
			NewCommand[Index] = 0;
		}
	}

	return Result;
}

/// @brief This function starts sending the data stored in TextToBeSent
/// The function writes the first byte to UART (FIFO input buffer of UART),
/// and copies the next bytes from TextToBeSent to UartOutputBuffer.
/// See the assumptions specified in the module description.
/// @param TextToBeSent pointer to a string (character with code zero cannot be sent)
/// @return 0 on success
/// @return -1 on failure
int8_t transmitViaSerialPort( const char* TextToBeSent ){
	bool Result = true;
	if (NULL == TextToBeSent){
		return -1; // improper value of the argument
	}
	char FirstCharacter = TextToBeSent[0];
	if (0 == FirstCharacter){
		return -1; // incorrect value pointed to by argument
	}

	if (!ringSpscIsEmpty( &OutputRingBuffer )){
		return -1; // The output buffer is not empty
	}

	if (is_tx_irq_enabled(UART_ID) || !uart_is_writable( UART_ID )){
		// TX UART interrupt is active (the UART is transmitting now)
    	return -1;	// writing to the buffer during transmission should not occur
	}
	// TX UART interrupt is not active now (the UART is not transmitting now)
	// Prepare data in the UartOutputBuffer (beginning from the 2nd byte of TextToBeSent)
	uint8_t Index = 1;
	while (Result && (0 != TextToBeSent[Index])){ // The terminating character (zero) should not be sent
		Result = ringSpscPush( &OutputRingBuffer, TextToBeSent[Index] );
		Index++;
	}
	uart_putc_raw( UART_ID, FirstCharacter ); 		// uart_putc is not good due to its CRLF support
	uart_set_irq_enables( UART_ID, true, true );	// the next bytes will be sent in the interrupt handler

	return 0;
}

static void serialPortInterruptHandler( void ){
	changeDebugPin1(true);

	uint16_t UartErrorTemporary = 0;
	if (uart_is_readable(UART_ID)){
		char IncomingCharacter = uart_getc(UART_ID);
		bool Result = ringSpscPush( &InputRingBuffer, IncomingCharacter );
		if( !Result){
			UartErrorTemporary |= UART_ERROR_INPUT_BUFFER_OVERFLOW;
		}
		if (uart_is_writable( UART_ID )){
			uart_putc_raw( UART_ID, IncomingCharacter ); // send echo
		}
		atomic_store_explicit( &WhenReceivedLastByte, time_us_64(), memory_order_relaxed ); // to check how long the silence lasts in the incoming transmission

		// Check if there is any outgoing transmission
		if (is_tx_irq_enabled(UART_ID) || !uart_is_writable( UART_ID ) || (!ringSpscIsEmpty( &OutputRingBuffer ))){
			UartErrorTemporary |= UART_WARNING_INCOMING_WHILE_OUTGOING;
		}
	}
	else{
		if((!ringSpscIsEmpty( &OutputRingBuffer )) && uart_is_writable( UART_ID )){
			uint8_t OutgoingData;
			ringSpscPop( &OutputRingBuffer, &OutgoingData );
			uart_putc_raw( UART_ID, OutgoingData );			// uart_putc is not good due to its CRLF support
		}
		if (ringSpscIsEmpty( &OutputRingBuffer )){
			uart_set_irq_enables( UART_ID, true, false );	// there is nothing more to send so stop interrupts from the sender
		}
	}
	atomic_fetch_or_explicit( &UartError, UartErrorTemporary, memory_order_relaxed );

	changeDebugPin1(false); // measured duration 1...6 us
}

static inline bool is_tx_irq_enabled(uart_inst_t *uart) {
	return (uart_get_hw(uart)->imsc & UART_UARTIMSC_TXIM_BITS) != 0;
}

