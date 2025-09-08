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

/// @brief This function initializes I2C port used to communicate with PCF8574
void initializePcf8574Outputs(void);

/// @brief This function is a debugging tool, normally not used
void testPcf8574(void);

#endif // SOURCE_PCF8574_OUTPUTS_H_
