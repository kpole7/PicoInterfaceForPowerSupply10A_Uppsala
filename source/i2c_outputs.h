// i2c_outputs.h
/// @file i2c_outputs
/// @brief This module implements the lower layer of communication with two PCF8574 via I2C
///
/// Outgoing transmission via I2C is used only.

#ifndef SOURCE_I2C_OUTPUTS_H_
#define SOURCE_I2C_OUTPUTS_H_

#include "pico/stdlib.h"

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes I2C port used to communicate with PCF8574
void initializeI2cOutputs(void);

/// @brief This function writes one byte to the PCF8574 IC.
/// @param I2cAddress the hardware address of one of two PCF8574 ICs
/// @param Value data to be stored in the PCF8574 (one byte)
bool i2cWrite( uint8_t I2cAddress, uint8_t Value);

#endif // SOURCE_I2C_OUTPUTS_H_
