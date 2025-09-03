// rstl_protocol.c

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "rstl_protocol.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_MINIMAL_LENGTH			3

#define NUMBER_OF_POWER_SUPPLIES		4

#define INITIAL_DAC_VALUE				0x800
#define INITIAL_MAIN_CONTACTOR_STATE	false

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This buffer is used to pass data from UART
char NewCommand[COMMAND_BUFFER_LENGTH];

/// @brief Currently selected (active) power supply unit
uint8_t SelectedChannel;

/// @brief Setpoint value sent to DAC
uint16_t RequiredDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief The state of the power contactor: true=power on; false=power off
bool MainContactorStateOn;




//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializeRstlProtocol(void){
	SelectedChannel = 0;
	for (uint8_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		RequiredDacValue[J] = INITIAL_DAC_VALUE;
	}
	MainContactorStateOn = INITIAL_MAIN_CONTACTOR_STATE;
}

uint8_t executeCommand(void){
	uint8_t ErrorCode = COMMAND_GOOD;
	float FloatingPointValue;
	unsigned UnsignedValue;
	int CommadLength = strlen( NewCommand );

	if (CommadLength < 3){
		ErrorCode = COMMAND_INCORRECT_FORMAT;
	}

	if (strstr(NewCommand, "PC") == NewCommand){ // "Program current" command
		int Result = sscanf( NewCommand, "PC%f\r\n", &FloatingPointValue );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_PC_INCORRECT_FORMAT;
		}
		else{
			if ((FloatingPointValue < -10.0) || (FloatingPointValue > 10.0)){
				ErrorCode = COMMAND_PC_INCORRECT_VALUE;
			}
			else{
				// essential action

			}
		}

		printf( "sscanf <%s>  %d  %f\n", NewCommand, ErrorCode, FloatingPointValue );
	}
	else if (strstr(NewCommand, "Z") == NewCommand){ // "Select channel" command
		int Result = sscanf( NewCommand, "Z%u\r\n", &UnsignedValue );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_Z_INCORRECT_FORMAT;
		}
		else{
			if ((0 == UnsignedValue) || (UnsignedValue > NUMBER_OF_POWER_SUPPLIES)){
				ErrorCode = COMMAND_Z_INCORRECT_VALUE;
			}
			else{
				// essential action
				SelectedChannel = UnsignedValue-1;
			}
		}

		printf( "sscanf <%s>  %d  %u\n", NewCommand, ErrorCode, UnsignedValue );
	}
	else{
		ErrorCode = COMMAND_UNKNOWN;
	}
	return ErrorCode;
}

