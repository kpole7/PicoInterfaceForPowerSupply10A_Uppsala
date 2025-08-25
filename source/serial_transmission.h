// serial_transmission.h
/// @file serial_transmission.h
/// @brief This module supports communication with the master unit via serial port
///
/// The module acts as a slave. It receives commands from the master unit and
/// sends the responses.  The protocol consists from the following text commands:
/// command			response
/// * command: `MC\r\n`    response: `n.n\r\n`
/// * command: `PCn\r\n`   response: `xyz`


#ifndef SOURCE_SERIAL_TRANSMISSION_H_
#define SOURCE_SERIAL_TRANSMISSION_H_

/// @brief This function initializes hardware port (UART) and initializes state machines for serial communication
void serialPortInitialization(void);

/// @brief This function drives the state machine that receives commands via serial port
void serialPortReceiver(void);


#endif /* SOURCE_SERIAL_TRANSMISSION_H_ */
