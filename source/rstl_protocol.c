/// @file rstl_protocol.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "rstl_protocol.h"
#include "uart_talks.h"
#include "writing_to_dac.h"
#include "psu_talks.h"
#include "adc_inputs.h"
#include "compilation_time.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define COMMAND_MINIMAL_LENGTH				3
#define COMMAND_FLOATING_POINT_MAX_LENGTH	9	// " -9.12345"
#define COMMAND_FLOATING_POINT_DIGITS_LIMIT	6
#define COMMAND_FLOATING_POINT_VALUE_LIMIT	10.0

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// @brief This buffer is used to pass data from UART
/// This buffer is filled by the uart_talks module and is interpreted by the rstl_protocol module
char NewCommand[COMMAND_BUFFER_LENGTH];

/// @brief Currently selected (active) power supply unit
/// All commands related to power supply settings apply to this device
atomic_uint_fast16_t UserSelectedChannel;

/// @brief This is a code of an action that cannot be executed immediately but must be processed by a state machine
/// The variable can be modified in the main loop and in the timer interrupt handler
atomic_uint_fast16_t OrderCode;

/// @brief This is a power supply unit to which OrderCode refers
/// The variable can be modified in the main loop and in the timer interrupt handler
atomic_uint_fast16_t OrderChannel;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

static int32_t parseFloatArgument( float *Result, char *TextPtr, char EndMark );

static int32_t parseOneDigitArgument( uint8_t *Result, char *TextPtr, char EndMark );

#if 0 // service commands
static int32_t parseHexadecimal3DigitsArgument( uint16_t *Result, char *TextPtr, char EndMark );
#endif

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes variables of this module
void initializeRstlProtocol(void){
	atomic_store_explicit( &UserSelectedChannel, 0, memory_order_release );
	for (uint8_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		atomic_store_explicit( &UserSetpointDacValue[J], OFFSET_IN_DAC_UNITS, memory_order_release );
		WrittenToDacValue[J] = OFFSET_IN_DAC_UNITS;
	}
	atomic_store_explicit( &OrderCode, ORDER_NONE, memory_order_release );
	atomic_store_explicit( &OrderChannel, 0, memory_order_release );
}

/// @brief This function is called in the main loop
void driveUserInterface(void){
#if SEND_I2C_ERROR_MESSAGE_ASYNCHRONOUSLY == 1
	if (atomic_load_explicit( &I2cErrorsDisplay, memory_order_acquire )){
		atomic_store_explicit( &I2cErrorsDisplay, false, memory_order_release );
		transmitViaSerialPort("\r\nI2C ERROR !\r\n>");
	}
#endif
	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_ACCEPTED){
		atomic_store_explicit( &OrderCode, ORDER_NONE, memory_order_release );
	}
	bool NewCommandIsReady = serialPortReceiver();
	if (NewCommandIsReady){
		executeCommand();
	}
}

/// @brief This function executes the command stored in NewCommand buffer
/// @return value from enum CommandErrors
CommandErrors executeCommand(void){
	char ResponseBuffer[LONGEST_RESPONSE_LENGTH];
	CommandErrors ErrorCode = COMMAND_PROPER;
	int32_t ParsingResult = 0;
	int CommadLength = strlen( NewCommand );

	if (CommadLength < 3){
		ErrorCode = COMMAND_INCORRECT_FORMAT;
	}

	if (strstr(NewCommand, "PC") == NewCommand){ // Program Current command
		float CommandFloatingPointArgument = 22222.2;
		int16_t ValueInDacUnits = 22222; // value in the case of failure (out of range)
		ParsingResult = parseFloatArgument( &CommandFloatingPointArgument, NewCommand+2, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 2+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			if ((CommandFloatingPointArgument < -COMMAND_FLOATING_POINT_VALUE_LIMIT) ||
					(CommandFloatingPointArgument > COMMAND_FLOATING_POINT_VALUE_LIMIT))
			{
				ErrorCode = COMMAND_INCORRECT_ARGUMENT;
			}
			else{
				if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_NONE){
					uint16_t TemporaryState = atomic_load_explicit(&PsuState, memory_order_acquire);
					// proper syntax; command: power up
					if (PSU_RUNNING == TemporaryState){
						// essential action
						ValueInDacUnits = (int16_t)round(CommandFloatingPointArgument * AMPERES_TO_DAC_COEFFICIENT);
						ValueInDacUnits += OFFSET_IN_DAC_UNITS;
						if (ValueInDacUnits < 0){
							ValueInDacUnits = 0;
						}
						if (FULL_SCALE_IN_DAC_UNITS < ValueInDacUnits){
							ValueInDacUnits = FULL_SCALE_IN_DAC_UNITS;
						}
						uint16_t TemporarySelectedChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
						if (TemporarySelectedChannel < NUMBER_OF_POWER_SUPPLIES){
							atomic_store_explicit( &UserSetpointDacValue[TemporarySelectedChannel], ValueInDacUnits, memory_order_release );
						}
						atomic_store_explicit( &OrderCode, ORDER_COMMAND_PC, memory_order_release );
						atomic_store_explicit( &OrderChannel, TemporarySelectedChannel, memory_order_release );
						transmitViaSerialPort(">");
					}
					else{
						ErrorCode = COMMAND_INVOKED_IN_INCONSISTENT_STATE;
					}
				}
				else{
					ErrorCode = COMMAND_OUT_OF_SERVICE;
				}
			}
		}
		printf( "%12llu\tPC\t%u\tE=%d\t%d\t0x%04X\t%d\n",
				time_us_64(),
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				ErrorCode,
				ValueInDacUnits-OFFSET_IN_DAC_UNITS, ValueInDacUnits,
				ParsingResult );
	}
	else if (strstr(NewCommand, "?PC") == NewCommand){ // "Get set-point value of current" command
		uint16_t TemporarySelectedChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
		if ((CommadLength != 3+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			float TemporaryUserSetpoint = (float)atomic_load_explicit( &UserSetpointDacValue[TemporarySelectedChannel], memory_order_acquire );
			TemporaryUserSetpoint -= (float)OFFSET_IN_DAC_UNITS;
			TemporaryUserSetpoint *= DAC_TO_AMPERES_COEFFICIENT;
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "%.2f\r\n>", TemporaryUserSetpoint );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd ?PC\tE=%d\tch=%u\t0x%04X\n", ErrorCode,
				(unsigned)TemporarySelectedChannel+1,
				atomic_load_explicit( &UserSetpointDacValue[TemporarySelectedChannel], memory_order_acquire ) );
	}
	else if (strstr(NewCommand, "Z") == NewCommand){ // "Select channel" command
		uint8_t TemporaryChannel;
		ParsingResult = parseOneDigitArgument( &TemporaryChannel, NewCommand+1, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 1+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			if ((0 == TemporaryChannel) || (TemporaryChannel > NUMBER_OF_POWER_SUPPLIES)){
				ErrorCode = COMMAND_INCORRECT_ARGUMENT;
			}
			else{
				// essential action
				atomic_store_explicit( &UserSelectedChannel, TemporaryChannel-1, memory_order_release );
				transmitViaSerialPort(">");
			}
		}
		printf( "cmd Z\tE=%d\tch=%u\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
	}
	else if (strstr(NewCommand, "?Z") == NewCommand){ // "Get selected channel number" command
		if ((CommadLength != 2+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "Z=%u\r\n>",
					(unsigned)(atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1) );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd ?Z\tE=%d\tch=%u\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
	}
	else if (strstr(NewCommand, "POWER") == NewCommand){ // "Switch power on/off" command
		uint8_t TemporaryPowerArgument;
		ParsingResult = parseOneDigitArgument( &TemporaryPowerArgument, NewCommand+5, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 5+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			if (TemporaryPowerArgument > 1){
				ErrorCode = COMMAND_INCORRECT_ARGUMENT;
			}
			else{
				// essential action
				int TemporaryState = atomic_load_explicit(&PsuState, memory_order_acquire);
				if (1 == TemporaryPowerArgument){
					// proper syntax; command: power up
					if (PSU_STOPPED == TemporaryState){
						atomic_store_explicit( &OrderCode, ORDER_COMMAND_POWER_UP, memory_order_release );
						transmitViaSerialPort(">");
					}
					else{
						ErrorCode = COMMAND_INVOKED_IN_INCONSISTENT_STATE;
					}
				}
				else{
					// proper syntax; command: power down
					if (PSU_RUNNING == TemporaryState){
						atomic_store_explicit( &OrderCode, ORDER_COMMAND_POWER_DOWN, memory_order_release );
						transmitViaSerialPort(">");
					}
					else{
						ErrorCode = COMMAND_INVOKED_IN_INCONSISTENT_STATE;
					}
				}
			}
		}
		printf( "cmd pow %d\tE=%d\n", TemporaryPowerArgument, ErrorCode );
	}
	else if (strstr(NewCommand, "?POWER") == NewCommand){ // "Get state of power switch" command
		if ((CommadLength != 6+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			if (atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire )){
				transmitViaSerialPort( "1\r\n>" );
			}
			else{
				transmitViaSerialPort( "0\r\n>" );
			}
		}
		if (atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire )){
			printf( "cmd ?pw\tE=%d\tch=%u\tpower on\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
		else{
			printf( "cmd ?pw\tE=%d\tch=%u\tpower off\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
	}
	else if (strstr(NewCommand, "MC") == NewCommand){ // "Measure current" command
		if ((CommadLength != 2+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "V=%f\r\n>",
					getVoltage( atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)>0? 1 : 0 ) );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd MC\tE=%d\tch=%u\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
	}
	else if (strstr(NewCommand, "VERSION") == NewCommand){ // "Get info about the current version" command
		if ((CommadLength != 7+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "ver. %s\r\n>", CompilationTime );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd wer\tE=%d\tch=%u\tver. %s\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				CompilationTime );
	}
	else if (strstr(NewCommand, "ST") == NewCommand){ // "Get Status" command
		if ((CommadLength != 2+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "sig2%s err i2c %u %u uart %X\r\n>",
					convertSig2TableToText(),
					(unsigned)atomic_load_explicit(&I2cConsecutiveErrors, memory_order_acquire),
					(unsigned)atomic_load_explicit(&I2cMaxConsecutiveErrors, memory_order_acquire),
					(unsigned)atomic_load_explicit(&UartError, memory_order_acquire) );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd st E=%d\n", ErrorCode );
	}
	else if (strstr(NewCommand, "RE") == NewCommand){ // "Reset Errors" command
		if ((CommadLength != 2+2) || (NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_INCORRECT_SYNTAX;
		}
		else{
			// essential action
			atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
			atomic_store_explicit( &I2cMaxConsecutiveErrors, 0, memory_order_release );
			atomic_store_explicit( &UartError, 0, memory_order_release );
			transmitViaSerialPort( "Resetting errors\r\n>" );
		}
		printf( "cmd re E=%d\n", ErrorCode );
	}
	else{
		ErrorCode = COMMAND_UNKNOWN;
		printf( "cmd ???\t" );
		for (int J=0; NewCommand[J] != 0; J++){
			printf( "%c", (NewCommand[J] >= ' ')? NewCommand[J] : '~' );
		}
		printf( "\n" );
	}
	if (COMMAND_PROPER != ErrorCode){
		snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "Error %d\r\n>", ErrorCode );
		transmitViaSerialPort( ResponseBuffer );
	}
	return ErrorCode;
}

static int32_t parseFloatArgument( float *Result, char *TextPtr, char EndMark ){
	float FloatArgument;
	uint8_t CharacterIndex = 0;
	uint8_t Spaces = 0;
	uint8_t Pluses = 0;
	uint8_t Minuses = 0;
	uint8_t Points = 0;
	uint8_t DecimalDigits = 0;

	while( CharacterIndex < COMMAND_FLOATING_POINT_MAX_LENGTH ){
		if (EndMark == TextPtr[CharacterIndex]){
			if (0 == DecimalDigits){
				// no digit
				return -12;
			}
			FloatArgument = atof( &TextPtr[Spaces] );
			*Result = FloatArgument;
			return CharacterIndex;
		}
		else if (' ' == TextPtr[CharacterIndex]){
			Spaces++;
			if (Spaces > 1){
				// too many spaces
				return -11;
			}
			if ((Pluses != 0) || (Minuses != 0) || (DecimalDigits != 0)){
				// improper position of space
				return -10;
			}
		}
		else if ('+' == TextPtr[CharacterIndex]){
			Pluses++;
			if (Pluses > 1){
				// too many pluses
				return -9;
			}
			if ((Minuses != 0) || (DecimalDigits != 0)){
				// improper position of plus
				return -8;
			}
		}
		else if ('-' == TextPtr[CharacterIndex]){
			Minuses++;
			if (Minuses > 1){
				// too many minuses
				return -7;
			}
			if ((Pluses != 0) || (DecimalDigits != 0)){
				// improper position of minus
				return -6;
			}
		}
		else if ('.' == TextPtr[CharacterIndex]){
			Points++;
			if (Points > 1){
				// too many points
				return -5;
			}
			if (0 == DecimalDigits){
				// improper position of point
				return -4;
			}
		}
		else if (('0' <= TextPtr[CharacterIndex]) && ('9' >= TextPtr[CharacterIndex])){
			DecimalDigits++;
			if (DecimalDigits > COMMAND_FLOATING_POINT_DIGITS_LIMIT){
				// too many digits
				return -3;
			}
		}
		else{
			// improper character
			return -2;
		}
		CharacterIndex++;
	}
	// improper length
	return -1;
}

static int32_t parseOneDigitArgument( uint8_t *Result, char *TextPtr, char EndMark ){
	uint8_t UInt8_Argument = 0;
	uint8_t CharacterIndex = 0;
	uint8_t Spaces = 0;
	uint8_t DecimalDigits = 0;

	while( CharacterIndex < 3 ){
		if (EndMark == TextPtr[CharacterIndex]){
			if (0 == DecimalDigits){
				// no digit
				return -5;
			}
			*Result = UInt8_Argument;
			return CharacterIndex;
		}
		else if (' ' == TextPtr[CharacterIndex]){
			Spaces++;
			if (Spaces > 1){
				// too many spaces
				return -4;
			}
		}
		else if (('0' <= TextPtr[CharacterIndex]) && ('9' >= TextPtr[CharacterIndex])){
			DecimalDigits++;
			if (DecimalDigits > 1){
				// too many digits
				return -3;
			}
			UInt8_Argument = (uint8_t)(TextPtr[CharacterIndex] - '0');
		}
		else{
			// improper character
			return -2;
		}
		CharacterIndex++;
	}
	// improper length
	return -1;
}
