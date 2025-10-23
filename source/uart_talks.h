/// @file uart_talks.h
/// @brief This module implements the lower layer of communication with the master unit via serial port
///
/// This module receives and sends data frames via a serial port.
/// It is assumed that this interface is a slave device that only responds to commands from the master device,
/// and that incoming and outgoing transmission takes place alternately.
/// If data is received on the UART during an outgoing transmission, the outgoing data transmission
/// should be stopped immediately (the master should never start sending a new command until the previous command has been completed).

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
/// @return true if a new command has been received via UART
/// @return false if there is no new command
bool serialPortReceiver(void);

/// @brief This function starts sending the data stored in TextToBeSent
/// The function writes the first byte to UART (FIFO input buffer of UART),
/// and copies the next bytes from TextToBeSent to UartOutputBuffer.
/// See the assumptions specified in the module description.
/// @param TextToBeSent pointer to a string (character with code zero cannot be sent)
/// @return 0 on success
/// @return -1 on failure
int8_t transmitViaSerialPort( const char* TextToBeSent );

#endif // SOURCE_UART_TALKS_H_
