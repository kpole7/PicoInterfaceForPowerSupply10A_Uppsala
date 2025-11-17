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
