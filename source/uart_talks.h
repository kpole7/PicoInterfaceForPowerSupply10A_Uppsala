/// @file uart_talks.h
/// @brief This module implements the lower layer of communication with the master unit via serial port
///
/// This module receives and sends data frames via a serial port

#ifndef SOURCE_UART_TALKS_H_
#define SOURCE_UART_TALKS_H_

#include "pico/stdlib.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define LONGEST_COMMAND_LENGTH				28			// ???

#define LONGEST_RESPONSE_LENGTH				60

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes hardware port (UART) and initializes state machines for serial communication
void serialPortInitialization(void);

/// @brief This function drives the state machine that receives frames via serial port
void serialPortReceiver(void);

/// @brief This function starts sending the data stored in UartOutputBuffer
/// The function copies the data from UartOutputBuffer to FIFO input buffer of UART
/// @param TextToBeSent pointer to a string (character with code zero cannot be sent)
/// @return 0 on success
/// @return -1 on failure
int8_t transmitViaSerialPort( const char* TextToBeSent );

#endif // SOURCE_UART_TALKS_H_
