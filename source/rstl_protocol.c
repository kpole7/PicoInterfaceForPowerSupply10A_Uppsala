// rstl_protocol.c

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "rstl_protocol.h"
#include "psu_talks.h"
#include "adc_inputs.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_MINIMAL_LENGTH			3

#define INITIAL_DAC_VALUE				0x800

#define AMPERES_TO_DAC_COEFFICIENT		(4096.0 / 20.0)
#define OFFSET_IN_DAC_UNITS				2048
#define FULL_SCALE_IN_DAC_UNITS			4095	// 4095 = 0xFFF

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

char NewCommand[COMMAND_BUFFER_LENGTH];

uint8_t SelectedChannel;

uint8_t AddressTable[NUMBER_OF_POWER_SUPPLIES];

OrderCodes OrderCode;

uint16_t RequiredDacValue[NUMBER_OF_POWER_SUPPLIES];
float RequiredAmperesValue[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief The state of the power contactor: true=power on; false=power off
bool IsMainContactorStateOn;

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializeRstlProtocol(void){
	SelectedChannel = 0;
	for (uint8_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		RequiredDacValue[J] = INITIAL_DAC_VALUE;
		RequiredAmperesValue[J] = 0.0;
	}
	OrderCode = ORDER_NONE;

	IsMainContactorStateOn = INITIAL_MAIN_CONTACTOR_STATE;

	assert( NUMBER_OF_POWER_SUPPLIES == 4 );
	AddressTable[0] = INITIAL_ADDRESS_1;
	AddressTable[1] = INITIAL_ADDRESS_2;
	AddressTable[2] = INITIAL_ADDRESS_3;
	AddressTable[3] = INITIAL_ADDRESS_4;
}

CommandErrors executeCommand(void){
	char ResponseBuffer[LONGEST_RESPONSE_LENGTH];
	CommandErrors ErrorCode = COMMAND_GOOD;
	int CommadLength = strlen( NewCommand );

	if (CommadLength < 3){
		ErrorCode = COMMAND_INCORRECT_FORMAT;
	}

	if (strstr(NewCommand, "PCX") == NewCommand){ // "Program current hexadecimal" command
		unsigned DacValue;
		int Result = sscanf( NewCommand, "PCX%X\r\n", &DacValue );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_PCX_INCORRECT_FORMAT;
		}
		else{
			// essential action
			if (SelectedChannel < NUMBER_OF_POWER_SUPPLIES){
				RequiredDacValue[SelectedChannel] = (uint16_t)DacValue;
				RequiredAmperesValue[SelectedChannel] = NAN;
			}
			OrderCode = ORDER_PCX;
			transmitViaSerialPort(">");
		}
		printf( "command <%s> E=%d ch=%d %04X\n", NewCommand, ErrorCode, SelectedChannel, DacValue );
	}
	else if (strstr(NewCommand, "PC") == NewCommand){ // "Program current" command
		float CommandFloatingPointArgument = NAN;
		int16_t ValueInDacUnits = 22222; // value in the case of failure (out of range)
		int Result = sscanf( NewCommand, "PC%f\r\n", &CommandFloatingPointArgument );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_PC_INCORRECT_FORMAT;
		}
		else{
			if ((CommandFloatingPointArgument < -10.0) || (CommandFloatingPointArgument > 10.0)){
				ErrorCode = COMMAND_PC_INCORRECT_VALUE;
			}
			else{
				// essential action
				ValueInDacUnits = (int16_t)round(CommandFloatingPointArgument * AMPERES_TO_DAC_COEFFICIENT);
				ValueInDacUnits += OFFSET_IN_DAC_UNITS;
				if (ValueInDacUnits < 0){
					ValueInDacUnits = 0;
				}
				if (FULL_SCALE_IN_DAC_UNITS < ValueInDacUnits){
					ValueInDacUnits = FULL_SCALE_IN_DAC_UNITS;
				}
				if (SelectedChannel < NUMBER_OF_POWER_SUPPLIES){
					RequiredDacValue[SelectedChannel] = (uint16_t)ValueInDacUnits;
					RequiredAmperesValue[SelectedChannel] = CommandFloatingPointArgument;
				}
				OrderCode = ORDER_PCX;
				transmitViaSerialPort(">");
			}
		}

		printf( "command <%s>  E=%d  %.3f > %d\n", NewCommand, ErrorCode, CommandFloatingPointArgument, ValueInDacUnits );
	}
	else if (strstr(NewCommand, "Z") == NewCommand){ // "Select channel" command
		unsigned TemporaryChannel;
		int Result = sscanf( NewCommand, "Z%u\r\n", &TemporaryChannel );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_Z_INCORRECT_FORMAT;
		}
		else{
			if ((0 == TemporaryChannel) || (TemporaryChannel > NUMBER_OF_POWER_SUPPLIES)){
				ErrorCode = COMMAND_Z_INCORRECT_VALUE;
			}
			else{
				// essential action
				SelectedChannel = TemporaryChannel-1;
				transmitViaSerialPort(">");
			}
		}

		printf( "command <%s>  E=%d  %u-1=?=%u\n", NewCommand, ErrorCode, TemporaryChannel, SelectedChannel );
	}
	else if (strstr(NewCommand, "?Z") == NewCommand){ // "Get selected channel number" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__Z_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "Z=%u\r\n>", (unsigned)(SelectedChannel+1) );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d  ch=%u\n", NewCommand, ErrorCode, SelectedChannel+1 );
	}
	else if (strstr(NewCommand, "POWER") == NewCommand){ // "Switch power on/off" command
		unsigned TemporaryPowerArgument;
		int Result = sscanf( NewCommand, "POWER%u\r\n", &TemporaryPowerArgument );
		if ((Result != 1) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_POWER_INCORRECT_FORMAT;
		}
		else{
			if (TemporaryPowerArgument > 1){
				ErrorCode = COMMAND_POWER_INCORRECT_VALUE;
			}
			else{
				// essential action
				if (1 == TemporaryPowerArgument){
					IsMainContactorStateOn = true;
				}
				else{
					IsMainContactorStateOn = false;
				}
				setMainContactorState( IsMainContactorStateOn );
				transmitViaSerialPort(">");
			}
		}

		printf( "command <%s>  E=%d  Arg=%u\n", NewCommand, ErrorCode, TemporaryPowerArgument );
	}
	else if (strstr(NewCommand, "?POWER") == NewCommand){ // "Get state of power switch" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__POWER_INCORRECT_FORMAT;
		}
		else{
			// essential action
			if (IsMainContactorStateOn){
				transmitViaSerialPort( "1\r\n>" );
			}
			else{
				transmitViaSerialPort( "0\r\n>" );
			}
		}

		printf( "command <%s>  E=%d  %c\n", NewCommand, ErrorCode, IsMainContactorStateOn?'1':'0' );
	}
	else if (strstr(NewCommand, "MC") == NewCommand){ // "Measure current" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_MC_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "V=%f\r\n>", getVoltage( SelectedChannel>0? 1 : 0 ) );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d  ch=%u\n", NewCommand, ErrorCode, SelectedChannel+1 );
	}
	else if (strstr(NewCommand, "MY") == NewCommand){ // "Get Sig2 value" command
		bool Sig2Value = false;
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_MY_INCORRECT_FORMAT;
		}
		else{
			// essential action
			Sig2Value = getLogicFeedbackFromPsu();
			if (Sig2Value){
				transmitViaSerialPort( "1\r\n>" );
			}
			else{
				transmitViaSerialPort( "0\r\n>" );
			}
		}

		printf( "command <%s>  E=%d  ch=%u Sig2=%c\n", NewCommand, ErrorCode, SelectedChannel+1, Sig2Value? '1':'0' );
	}
	else if (strstr(NewCommand, "ADDR") == NewCommand){ // "Set values of addresses" command
		unsigned TemporaryAddressArgument1, TemporaryAddressArgument2, TemporaryAddressArgument3, TemporaryAddressArgument4;
		int Result = sscanf( NewCommand, "ADDR:%X;%X;%X;%X\r\n",
				&TemporaryAddressArgument1, &TemporaryAddressArgument2, &TemporaryAddressArgument3, &TemporaryAddressArgument4 );
		if ((Result != 4) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_ADDRESS_INCORRECT_FORMAT;
		}
		else{
			if ((TemporaryAddressArgument1 > 0xFF) || (TemporaryAddressArgument2 > 0xFF) ||
					(TemporaryAddressArgument3 > 0xFF) || (TemporaryAddressArgument4 > 0xFF))
			{
				ErrorCode = COMMAND_ADDRESS_INCORRECT_VALUE;
			}
			else{
				// essential action
				assert( NUMBER_OF_POWER_SUPPLIES == 4 );
				AddressTable[0] = (uint8_t)TemporaryAddressArgument1;
				AddressTable[1] = (uint8_t)TemporaryAddressArgument2;
				AddressTable[2] = (uint8_t)TemporaryAddressArgument3;
				AddressTable[3] = (uint8_t)TemporaryAddressArgument4;

				transmitViaSerialPort(">");
			}
		}

		printf( "command <%s>  E=%d  Arg=%04X %04X %04X %04X\n", NewCommand, ErrorCode,
				TemporaryAddressArgument1, TemporaryAddressArgument2, TemporaryAddressArgument3, TemporaryAddressArgument4 );
	}
	else if (strstr(NewCommand, "?ADDR") == NewCommand){ // "Get values of addresses" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__ADDRESS_INCORRECT_FORMAT;
		}
		else{
			// essential action
			assert( NUMBER_OF_POWER_SUPPLIES == 4 );
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "Addr:%04X;%04X;%04X;%04X\r\n>",
					AddressTable[0], AddressTable[1], AddressTable[2], AddressTable[3] );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d [...]\n", NewCommand, ErrorCode );
	}
	else{
		ErrorCode = COMMAND_UNKNOWN;
		printf( "command <%s>  %d\n", NewCommand, ErrorCode );
	}
	return ErrorCode;
}

