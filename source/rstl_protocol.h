// rstl_protocol.h
/// @file rstl_protocol.h
/// @brief This module provides a higher layer of communication with the master unit
///
/// The module acts as a slave. It receives commands from the master unit and
/// sends the responses.  The protocol consists from the following text commands:
///
/// 1.1. Command **Measure Current**: `MC\r\n`
///
/// 1.2. Exemplary response: `-10.34\r\n\n>`
///
/// 2.1. Exemplary command **Program Current**: `PC-5.67\r\n`
///
/// 2.2. Response: `\r\n\n>`
///
/// 3.1. Command **Place software revision**: `?M\r\n`
///
/// 3.2. Exemplary response: `Rev. 111.222.333 2025-12-31 23:59:59\r\n\n>`
///
/// 4.1. Command **Current DAC programming value**: `?C\r\n`
///
/// 4.2. Exemplary response: `-2.34\r\n\n>`
///
/// 5.1. Command **Get current direction**: `?Y\r\n`
///
/// 5.2. Exemplary response: `1\r\n\n>`
///
/// 6.1. Exemplary command **Select channel**: `Z2\r\n`
///
/// 6.2. Response: `\r\n\n>`
///
/// 6.1. Command **Get channel**: `?Z\r\n`
///
/// 6.2. Exemplary response: `1\r\n\n>`
///

#ifndef SOURCE_RSTL_PROTOCOL_H_
#define SOURCE_RSTL_PROTOCOL_H_

#include "uart_talks.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_BUFFER_LENGTH	(LONGEST_COMMAND_LENGTH+10)

//---------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------

typedef enum OrderCodesEnum{
	ORDER_NONE		= 0,
	ORDER_PCX		= 1,
	ORDER_PC		= 2,
	ORDER_Z			= 3,
	ORDER_MC		= 4,
	ORDER_MY		= 5,
} OrderCodes;

typedef enum CommandErrorsEnum{
	COMMAND_GOOD				= 0,
	COMMAND_UNKNOWN				= 1,
	COMMAND_INCORRECT_FORMAT	= 2,
	COMMAND_PCX_INCORRECT_FORMAT= 3,
	COMMAND_PC_INCORRECT_FORMAT	= 4,
	COMMAND_PC_INCORRECT_VALUE	= 5,
	COMMAND_Z_INCORRECT_FORMAT	= 6,
	COMMAND_Z_INCORRECT_VALUE	= 7,
	COMMAND__Z_INCORRECT_FORMAT	= 8,
	COMMAND_MC_INCORRECT_FORMAT	= 9,
	COMMAND_MY_INCORRECT_FORMAT	= 9,
} CommandErrors;

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This buffer is used to pass data from UART
/// This buffer is filled by the uart_talks module and is interpreted by the rstl_protocol module
extern char NewCommand[COMMAND_BUFFER_LENGTH];

/// @brief Currently selected (active) power supply unit
/// All commands related to power supply settings apply to this device
extern uint8_t SelectedChannel;

extern OrderCodes OrderCode;

extern float CommandFloatingPointArgument;

extern unsigned CommandUnsignedArgument;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes variables of this module
void initializeRstlProtocol(void);

/// @brief This function executes the command stored in NewCommand buffer
/// @return value from enum CommandErrors
CommandErrors executeCommand(void);

#endif // SOURCE_RSTL_PROTOCOL_H_
