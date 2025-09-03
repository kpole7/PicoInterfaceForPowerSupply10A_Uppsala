// rstl_protocol.c

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "rstl_protocol.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_MINIMAL_LENGTH		3

//---------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------

enum CommandErrors{
	COMMAND_GOOD				= 0,
	COMMAND_UNKNOWN				= 1,
	COMMAND_INCORRECT_FORMAT	= 2,
	COMMAND_PC_INCORRECT_FORMAT	= 3,
	COMMAND_Z_INCORRECT_FORMAT	= 4,
};

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

char NewCommand[COMMAND_BUFFER_LENGTH];

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

uint8_t executeCommand(void){
	uint8_t ErrorCode = COMMAND_GOOD;
	float FloatingPointValue;
	unsigned UnsignedValue;
	int CommadLength = strlen( NewCommand );

	if (CommadLength < 3){
		ErrorCode = COMMAND_INCORRECT_FORMAT;
	}

	if (strstr(NewCommand, "PC") != NULL){
		int Result = sscanf( NewCommand, "PC%f\r\n", &FloatingPointValue );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_PC_INCORRECT_FORMAT;
		}

		printf( "sscanf <%s>  %d  %f\n", NewCommand, ErrorCode, FloatingPointValue );
	}
	else if (strstr(NewCommand, "Z") != NULL){
		int Result = sscanf( NewCommand, "Z%u\r\n", &UnsignedValue );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_Z_INCORRECT_FORMAT;
		}

		printf( "sscanf <%s>  %d  %u\n", NewCommand, ErrorCode, UnsignedValue );
	}
	else{
		ErrorCode = COMMAND_UNKNOWN;
	}
	return ErrorCode;
}

