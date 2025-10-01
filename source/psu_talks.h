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

/// @brief This function is called periodically by the time interrupt handler
void psuTalksTimeTick(void);


#endif // SOURCE_PSU_TALKS_H_
