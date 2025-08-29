// serial_transmission.h
/// @file serial_transmission.h
/// @brief This module implements the lower layer of communication with the master unit via serial port
///
/// This module receives and sends data frames via a serial port


#ifndef SOURCE_SERIAL_TRANSMISSION_H_
#define SOURCE_SERIAL_TRANSMISSION_H_

/// @brief This function initializes hardware port (UART) and initializes state machines for serial communication
void serialPortInitialization(void);

/// @brief This function drives the state machine that receives frames via serial port
void serialPortReceiver(void);


void serialPortTransmitter(void);

void testSending(void);


#endif /* SOURCE_SERIAL_TRANSMISSION_H_ */
