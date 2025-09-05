// pcf8574_outputs.h
/// @file pcf8574_outputs
/// @brief This module implements the lower layer of communication with two PCF8574 via I2C (only outgoing transmission)
///
///

#ifndef SOURCE_PCF8574_OUTPUTS_H_
#define SOURCE_PCF8574_OUTPUTS_H_

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

void initializePcf8574Outputs(void);

//bool pcf8574_write( uint8_t I2cAddress, uint8_t Value);

void testPcf8574(void);

#endif // SOURCE_PCF8574_OUTPUTS_H_
