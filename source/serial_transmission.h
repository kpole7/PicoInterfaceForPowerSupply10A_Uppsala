// serial_transmission.h
/// @file serial_transmission.h
/// @brief This module supports communication with the master unit via serial port
///
/// The module acts as a slave. It receives commands from the master unit and
/// sends the responses.  The protocol consists from the following text commands:
///
/// 1.1. Command **Measure Current**: `MC\r\n`
///
/// 1.2. Exemplary response: `-10.34\r\n\n>`
///
/// 2.1. Exemplary command **Program Current**: `PC-5.67\r\n`
///
/// 2.2. Response: `\r\n\n>`
///
/// 3.1. Command **Place software revision**: `?M\r\n`
///
/// 3.2. Exemplary response: `Rev. abcdefghijklmnopqrst\r\n\n>`
///
/// 4.1. Command **Current DAC programming value**: `?C\r\n`
///
/// 4.2. Exemplary response: `-2.34\r\n\n>`
///
/// 5.1. Command **Logic output value**: `?Y\r\n`
///
/// 5.2. Exemplary response: `1\r\n\n>`
///


#ifndef SOURCE_SERIAL_TRANSMISSION_H_
#define SOURCE_SERIAL_TRANSMISSION_H_

/// @brief This function initializes hardware port (UART) and initializes state machines for serial communication
void serialPortInitialization(void);

/// @brief This function drives the state machine that receives commands via serial port
void serialPortReceiver(void);


#endif /* SOURCE_SERIAL_TRANSMISSION_H_ */
