/// @file rstl_protocol.c

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "rstl_protocol.h"
#include "uart_talks.h"
#include "psu_talks.h"
#include "adc_inputs.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_MINIMAL_LENGTH			3

#define INITIAL_DAC_VALUE				0x800

#define AMPERES_TO_DAC_COEFFICIENT		(4096.0 / 20.0)
#define DAC_TO_AMPERES_COEFFICIENT		(20.0 / 4096.0)
#define OFFSET_IN_DAC_UNITS				2048
#define FULL_SCALE_IN_DAC_UNITS			4095	// 4095 = 0xFFF

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This buffer is used to pass data from UART
/// This buffer is filled by the uart_talks module and is interpreted by the rstl_protocol module
char NewCommand[COMMAND_BUFFER_LENGTH];

/// @brief Currently selected (active) power supply unit
/// All commands related to power supply settings apply to this device
atomic_int SelectedChannel;

/// @brief This is a code of an action that cannot be executed immediately but must be processed by a state machine
/// The variable can be modified in the main loop and in the timer interrupt handler
atomic_int OrderCode;

/// @brief Setpoint value for a DAC
volatile uint16_t RequiredDacValue[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief The state of the power contactor: true=power on; false=power off
static bool IsMainContactorStateOn;

static float RequiredAmperesValue[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes GPIO controlling the the power contactor and initializes variables of this module
void initializeRstlProtocol(void){
	atomic_store_explicit( &SelectedChannel, 0, memory_order_release );
	for (uint8_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		RequiredDacValue[J] = INITIAL_DAC_VALUE;
		RequiredAmperesValue[J] = 0.0;
	}
	atomic_store_explicit( &OrderCode, ORDER_NONE, memory_order_release );

	IsMainContactorStateOn = INITIAL_MAIN_CONTACTOR_STATE;
}

/// @brief This function is called in the main loop
void driveUserInterface(void){
	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMPLETED){
		atomic_store_explicit( &OrderCode, ORDER_NONE, memory_order_release );
	}
	bool NewCommandIsReady = serialPortReceiver();
	if (NewCommandIsReady){
		executeCommand();
	}
}

/// @brief This function executes the command stored in NewCommand buffer
/// @return value from enum CommandErrors
/// @todo exception handling
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
			if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_NONE){
				// essential action
				if (atomic_load_explicit(&SelectedChannel, memory_order_acquire) < NUMBER_OF_POWER_SUPPLIES){
					RequiredDacValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] = (uint16_t)DacValue;
					RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] = (float)DacValue;
					RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] -= (float)OFFSET_IN_DAC_UNITS;
					RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] *= DAC_TO_AMPERES_COEFFICIENT;
				}
				atomic_store_explicit( &OrderCode, ORDER_COMMAND_PC, memory_order_release );
				transmitViaSerialPort(">");
			}
			else{
				ErrorCode = COMMAND_I2C_ERROR;
			}
		}
		printf( "command <%s> E=%d ch=%u %04X\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1, DacValue );
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
				if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_NONE){
					// essential action
					ValueInDacUnits = (int16_t)round(CommandFloatingPointArgument * AMPERES_TO_DAC_COEFFICIENT);
					ValueInDacUnits += OFFSET_IN_DAC_UNITS;
					if (ValueInDacUnits < 0){
						ValueInDacUnits = 0;
					}
					if (FULL_SCALE_IN_DAC_UNITS < ValueInDacUnits){
						ValueInDacUnits = FULL_SCALE_IN_DAC_UNITS;
					}
					if (atomic_load_explicit(&SelectedChannel, memory_order_acquire) < NUMBER_OF_POWER_SUPPLIES){
						RequiredDacValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] = (uint16_t)ValueInDacUnits;
						RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] = CommandFloatingPointArgument;
					}
					atomic_store_explicit( &OrderCode, ORDER_COMMAND_PC, memory_order_release );
					transmitViaSerialPort(">");
				}
				else{
					ErrorCode = COMMAND_I2C_ERROR;
				}
			}
		}

		printf( "command <%s>  E=%d  %.3f > %d\n", NewCommand, ErrorCode, CommandFloatingPointArgument, ValueInDacUnits );
	}
	else if (strstr(NewCommand, "?PC") == NewCommand){ // "Get set-point value of current" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__PC_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "%.2f\r\n>", RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d  ch=%u val=%f\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1,
				RequiredAmperesValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] );
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
				atomic_store_explicit( &SelectedChannel, TemporaryChannel-1, memory_order_release );
				transmitViaSerialPort(">");
			}
		}

		printf( "command <%s>  E=%d  ch=%u\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1 );
	}
	else if (strstr(NewCommand, "?Z") == NewCommand){ // "Get selected channel number" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__Z_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "Z=%u\r\n>",
					(unsigned)(atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1) );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d  ch=%u\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1 );
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
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "V=%f\r\n>",
					getVoltage( atomic_load_explicit(&SelectedChannel, memory_order_acquire)>0? 1 : 0 ) );
			transmitViaSerialPort( ResponseBuffer );
		}

		printf( "command <%s>  E=%d  ch=%u\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1 );
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

		printf( "command <%s>  E=%d  ch=%u Sig2=%c\n", NewCommand, ErrorCode,
				(unsigned)atomic_load_explicit(&SelectedChannel, memory_order_acquire)+1, Sig2Value? '1':'0' );
	}
	else{
		ErrorCode = COMMAND_UNKNOWN;
		printf( "command <%s>  %d\n", NewCommand, ErrorCode );
	}
	return ErrorCode;
}

