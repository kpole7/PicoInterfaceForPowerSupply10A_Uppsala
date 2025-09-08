// i2c_outputs.h
/// @file i2c_outputs
/// @brief This module implements the lower layer of communication with two PCF8574 via I2C (only outgoing transmission is used)
///
///

#ifndef SOURCE_I2C_OUTPUTS_H_
#define SOURCE_I2C_OUTPUTS_H_

#include "pico/stdlib.h"

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes I2C port used to communicate with PCF8574
void initializeI2cOutputs(void);

bool i2cWrite( uint8_t I2cAddress, uint8_t Value);

#endif // SOURCE_I2C_OUTPUTS_H_
