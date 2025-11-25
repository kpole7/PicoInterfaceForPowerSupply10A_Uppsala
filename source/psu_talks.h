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
// Global constants
//---------------------------------------------------------------------------------------------------

/// This definition contains a list of states of a finite state machine that represents entire multichannel power supply.
/// The state machine supports all operating modes of the equipment.
typedef enum {
	PSU_STOPPED,
	PSU_INITIAL_TEST_SIG2_LOW,
	PSU_INITIAL_TEST_SIG2_HIGH,
	PSU_INITIAL_ZEROING,
	PSU_INITIAL_CONTACTOR_ON,
	PSU_RUNNING,
	PSU_SHUTTING_DOWN_ZEROING,
	PSU_SHUTTING_DOWN_CONTACTOR_OFF,
}PsuOperatingStates;

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

extern atomic_int PsuState;

/// @brief Setpoint value for a DAC
extern volatile uint16_t UserSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Setpoint value for the DAC (number from 0 to 0xFFF) at a given moment (follows the ramp)
extern volatile uint16_t InstantaneousSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Set-point value written to the DAC (number from 0 to 0xFFF)
extern volatile uint16_t WrittenToDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief This variable is used in a simple state machine
extern volatile bool WritingToDac_IsValidData[NUMBER_OF_POWER_SUPPLIES];

/// @brief The state of the power contactor: true=power on; false=power off
extern bool IsMainContactorStateOn;

/// This array is used to store readings of Sig2 for each channel and
/// for two DAC values: 0 and FULL_SCALE_IN_DAC_UNITS
extern volatile bool Sig2LastReadings[NUMBER_OF_POWER_SUPPLIES][2];

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializePsuTalks(void);

/// @brief This function changes the power contactor state
void setMainContactorState( bool IsMainContactorStateOn );

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void );

/// @brief This function supervises ramp execution after a step has been completed and handles orders for DACs
/// @param Channel channel served in the last cycle
void psuStateMachine( uint32_t Channel );

#endif // SOURCE_PSU_TALKS_H_
