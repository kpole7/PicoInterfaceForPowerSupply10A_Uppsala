// serial_transmission.h
/// @file serial_transmission.h
/// @brief This module implements the lower layer of communication with the master unit via serial port
///
/// This module receives and sends data frames via a serial port

#ifndef SOURCE_SERIAL_TRANSMISSION_H_
#define SOURCE_SERIAL_TRANSMISSION_H_

#include "pico/stdlib.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define LONGEST_COMMAND_LENGTH				12			// ???

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


#endif // SOURCE_SERIAL_TRANSMISSION_H_
