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
// Macro directives
//---------------------------------------------------------------------------------------------------

#define INITIAL_MAIN_CONTACTOR_STATE	false

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializePsuTalks(void);

/// @brief This function drives the state machines of each PSU.
/// Each PSU has its own state machine, which allows them to operate simultaneously.
/// All state machines are identical.
/// This function is called periodically by the time interrupt handler.
void writeToDacStateMachine(void);

/// @brief This function changes the power contactor state
void setMainContactorState( bool IsMainContactorStateOn );

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void );

#endif // SOURCE_PSU_TALKS_H_
