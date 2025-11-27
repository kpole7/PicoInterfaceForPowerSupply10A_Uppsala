/// @file rstl_protocol.h
/// @brief This module provides a higher layer of communication with the master unit
///
/// The module acts as a slave. It receives commands from the master unit and
/// sends the responses.

#ifndef SOURCE_RSTL_PROTOCOL_H_
#define SOURCE_RSTL_PROTOCOL_H_

#include <stdatomic.h>
#include "config.h"
#include "uart_talks.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_BUFFER_LENGTH			(LONGEST_COMMAND_LENGTH+20)

#define ORDER_NONE						0
#define ORDER_ACCEPTED					1
#define ORDER_COMMAND_PCI				2	// Program Current Immediately
#define ORDER_COMMAND_PC				3	// Program Current (following ramp)
#define ORDER_COMMAND_POWER_UP			4
#define ORDER_COMMAND_POWER_DOWN		5
#define ORDER_COMMAND_ILLEGAL_CODE		6

#define AMPERES_TO_DAC_COEFFICIENT		(4096.0 / 20.0)
#define DAC_TO_AMPERES_COEFFICIENT		(20.0 / 4096.0)
#define FULL_SCALE_IN_DAC_UNITS			4095	// 4095 = 0xFFF

//---------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------

typedef enum CommandErrorsEnum{
	COMMAND_PROPER,
	COMMAND_UNKNOWN,
	COMMAND_INCORRECT_FORMAT,
	COMMAND_OUT_OF_SERVICE,
	COMMAND_INCORRECT_SYNTAX,
	COMMAND_INCORRECT_ARGUMENT,
	COMMAND_INVOKED_IN_INCONSISTENT_STATE,
} CommandErrors;

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This buffer is used to pass data from UART
/// This buffer is filled by the uart_talks module and is interpreted by the rstl_protocol module
extern char NewCommand[COMMAND_BUFFER_LENGTH];

/// @brief Currently selected (active) power supply unit
/// All commands related to power supply settings apply to this device
extern atomic_int UserSelectedChannel;

/// @brief This is a code of an action that cannot be executed immediately but must be processed by a state machine
/// The variable can be modified in the main loop and in the timer interrupt handler
extern atomic_int OrderCode;

/// @brief This is a power supply unit to which OrderCode refers
/// The variable can be modified in the main loop and in the timer interrupt handler
extern atomic_int OrderChannel;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes GPIO controlling the the power contactor and initializes variables of this module
void initializeRstlProtocol(void);

void driveUserInterface(void);

/// @brief This function executes the command stored in NewCommand buffer
/// @return value from enum CommandErrors
CommandErrors executeCommand(void);

#endif // SOURCE_RSTL_PROTOCOL_H_
