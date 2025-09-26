// psu_talks.h
/// @file psu_talks.h
/// @brief This module provides a higher layer of communication with the power supply units
///
/// The module uses hardware ports like I2C and GPIOs to control PSUs.
/// It executes commands from the rstl_protocol module.

#ifndef SOURCE_PSU_TALKS_H_
#define SOURCE_PSU_TALKS_H_

#include "pico/stdlib.h"
#include "rstl_protocol.h"

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes "not WR" output port used to communicate with PSUs
void initializePsuTalks(void);

/// @brief This function is used to place an order for psu_talks module to program a given DAC
///
/// The function writes a setpoint value to the PSU that is pointed by the SelectedChannel variable.
/// @param DacValue raw value (12-bit) to be stored in the DAC in the selected PSU
/// @return true on success
/// @return false on failure
bool writeToDac( uint16_t DacValue );

/// @brief This function is called periodically by the time interrupt handler
void psuTalksTimeTick(void);




#endif // SOURCE_PSU_TALKS_H_
