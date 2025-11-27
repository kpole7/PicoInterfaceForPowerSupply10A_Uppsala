/// @file writing_to_dac.h
/// @brief This module implements the hardware layer of communication with DACs.
/// This layer is lower than the protocol layer, but higher than the communication layer
/// with two PCF8574 chips the I2C interface.

#ifndef SOURCE_WRITING_TO_DAC_H_
#define SOURCE_WRITING_TO_DAC_H_

#include <stdatomic.h>
#include "pico/stdlib.h"
#include "config.h"

//---------------------------------------------------------------------------------------------------
// global variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used to monitor the I2C devices.
/// This flag is set if the number of consecutive errors exceeds the I2C_ERRORS_DISPLAY_LIMIT limit,
/// and cleared after the message is printed.
extern atomic_bool I2cErrorsDisplay;

/// @brief This variable is used to monitor the I2C devices.
/// This is the instantaneous value of the length of the i2c hardware error sequence.
extern atomic_int I2cConsecutiveErrors;

/// @brief This variable is used to monitor the I2C devices.
/// This is the longest recorded length of i2c hardware error sequences.
extern atomic_int I2cMaxConsecutiveErrors;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializeWritingToDacs(void);

/// @brief This function provides DAC write support for all power supplies
/// This function drives the state machines of each PSU. Each PSU has its own state machine,
/// which allows them to operate simultaneously. All state machines are identical.
/// This function is called periodically by the time interrupt handler.
void writeToDacStateMachine(void);

#endif /* SOURCE_WRITING_TO_DAC_H_ */
