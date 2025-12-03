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

#define SIG2_FOR_0_DAC_SETTING			0
#define SIG2_FOR_FULL_SCALE_DAC_SETTING	1
#define SIG2_IS_VALID_INFORMATION		2
#define SIG2_RECORD_SIZE				3

//---------------------------------------------------------------------------------------------------
// Global constants
//---------------------------------------------------------------------------------------------------

/// This definition contains a list of states of a finite state machine that represents entire multichannel power supply.
/// The state machine supports all operating modes of the equipment.
typedef enum {
	PSU_STOPPED,					// stable state; power supply is turned off
	PSU_INITIAL_SIG2_LOW_SET_DAC,	// transitional state; during power-up
	PSU_INITIAL_SIG2_LOW_TEST,		// transitional state; during power-up
	PSU_INITIAL_SIG2_HIGH_SET_DAC,	// transitional state; during power-up
	PSU_INITIAL_SIG2_HIGH_TEST,		// transitional state; during power-up
	PSU_INITIAL_ZEROING,			// transitional state; during power-up
	PSU_INITIAL_CONTACTOR_ON,		// transitional state; during power-up
	PSU_RUNNING,					// stable state; power supply is turned on
	PSU_SHUTTING_DOWN_ZEROING,		// transitional state; during power-down
	PSU_SHUTTING_DOWN_CONTACTOR_OFF,// transitional state; during power-down
	PSU_ILLEGAL_STATE				// number of correct states
}PsuOperatingStates;

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// FSM state; takes values from PsuOperatingStates
extern atomic_uint_fast16_t PsuState;

/// @brief User's set-point value for the DAC (number from 0 to 0xFFF)
extern atomic_uint_fast16_t UserSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Setpoint value for the DAC (number from 0 to 0xFFF) at a given moment (follows the ramp)
extern uint16_t InstantaneousSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Set-point value written to the DAC (number from 0 to 0xFFF)
extern uint16_t WrittenToDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief This variable is used in a simple state machine
extern bool WriteToDacDataReady[NUMBER_OF_POWER_SUPPLIES];

/// @brief The state of the power contactor: true=power on; false=power off
extern atomic_bool IsMainContactorStateOn;

/// This array is used to store readings of Sig2 for each channel and
/// for two DAC values: 0 and FULL_SCALE_IN_DAC_UNITS; additionally, a flag is used to indicate that the data is valid
extern atomic_bool Sig2LastReadings[NUMBER_OF_POWER_SUPPLIES][SIG2_RECORD_SIZE];

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializePsuTalks(void);

/// @brief This function changes the power contactor state
void setMainContactorState( bool NewState );

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void );

/// @brief This function supervises ramp execution after a step has been completed and handles orders for DACs
/// @param Channel channel served in the last cycle
/// @return true = reset channel index
/// @return false = don't modify channel index
bool psuStateMachine( uint32_t Channel );

/// This function prepares information on Sig2 readings in text form
char* convertSig2TableToText(void);

#endif // SOURCE_PSU_TALKS_H_
