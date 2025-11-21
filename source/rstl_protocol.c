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
atomic_int UserSelectedChannel;

/// @brief This is a code of an action that cannot be executed immediately but must be processed by a state machine
/// The variable can be modified in the main loop and in the timer interrupt handler
atomic_int OrderCode;

/// @brief This is a power supply unit to which OrderCode refers
/// The variable can be modified in the main loop and in the timer interrupt handler
atomic_int OrderChannel;

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

static float RequiredAmperesValue[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

static int32_t parseFloatArgument( float *Result, char *TextPtr, char EndMark );

static int32_t parseOneDigitArgument( uint8_t *Result, char *TextPtr, char EndMark );

static int32_t parseHexadecimal3DigitsArgument( uint16_t *Result, char *TextPtr, char EndMark );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes variables of this module
void initializeRstlProtocol(void){
	atomic_store_explicit( &UserSelectedChannel, 0, memory_order_release );
	for (uint8_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		UserSetpointDacValue[J] = OFFSET_IN_DAC_UNITS;
		WrittenToDacValue[J] = OFFSET_IN_DAC_UNITS;
		RequiredAmperesValue[J] = 0.0;
	}
	atomic_store_explicit( &OrderCode, ORDER_NONE, memory_order_release );
	atomic_store_explicit( &OrderChannel, 0, memory_order_release );
}

/// @brief This function is called in the main loop
void driveUserInterface(void){
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
/// @todo exception handling
CommandErrors executeCommand(void){
	char ResponseBuffer[LONGEST_RESPONSE_LENGTH];
	CommandErrors ErrorCode = COMMAND_GOOD;
	int32_t ParsingResult = 0;
	int CommadLength = strlen( NewCommand );

	if (CommadLength < 3){
		ErrorCode = COMMAND_INCORRECT_FORMAT;
	}

	if (strstr(NewCommand, "PCXI") == NewCommand){ // "Program current hexadecimal" command
		uint16_t DacValue;
		ParsingResult = parseHexadecimal3DigitsArgument( &DacValue, NewCommand+4, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 4+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_PCXI_INCORRECT_FORMAT;
		}
		else{
			if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_NONE){
				// essential action
				int TemporarySelectedChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
				if (TemporarySelectedChannel < NUMBER_OF_POWER_SUPPLIES){
					UserSetpointDacValue[TemporarySelectedChannel] = DacValue;
					RequiredAmperesValue[TemporarySelectedChannel] = (float)DacValue;
					RequiredAmperesValue[TemporarySelectedChannel] -= (float)OFFSET_IN_DAC_UNITS;
					RequiredAmperesValue[TemporarySelectedChannel] *= DAC_TO_AMPERES_COEFFICIENT;
				}
				atomic_store_explicit( &OrderCode, ORDER_COMMAND_PCI, memory_order_release );
				atomic_store_explicit( &OrderChannel, TemporarySelectedChannel, memory_order_release );
				transmitViaSerialPort(">");
			}
			else{
				ErrorCode = COMMAND_OUT_OF_SERVICE;
			}
		}
		printf( "cmd PCXI\tE=%d\tch=%u\t%d\t0x%04X\n",
				ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				(unsigned)(DacValue-OFFSET_IN_DAC_UNITS),
				(unsigned)DacValue );
	}
	else if (strstr(NewCommand, "PCI") == NewCommand){ // "Program current" command
		float CommandFloatingPointArgument = 22222.2;
		int16_t ValueInDacUnits = 22222; // value in the case of failure (out of range)
		ParsingResult = parseFloatArgument( &CommandFloatingPointArgument, NewCommand+3, '\r' );

#if 0
		printf( "Parsing res=%d, arg=%f\n", ParsingResult, CommandFloatingPointArgument );
#endif

		if ((ParsingResult < 0) || (CommadLength != 3+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_PCI_INCORRECT_FORMAT;
		}
		else{
			if ((CommandFloatingPointArgument < -COMMAND_FLOATING_POINT_VALUE_LIMIT) ||
					(CommandFloatingPointArgument > COMMAND_FLOATING_POINT_VALUE_LIMIT))
			{
				ErrorCode = COMMAND_PCI_INCORRECT_VALUE;
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
					int TemporarySelectedChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
					if (TemporarySelectedChannel < NUMBER_OF_POWER_SUPPLIES){
						UserSetpointDacValue[TemporarySelectedChannel] = (uint16_t)ValueInDacUnits;
						RequiredAmperesValue[TemporarySelectedChannel] = CommandFloatingPointArgument;
					}
					atomic_store_explicit( &OrderCode, ORDER_COMMAND_PCI, memory_order_release );
					atomic_store_explicit( &OrderChannel, TemporarySelectedChannel, memory_order_release );
					transmitViaSerialPort(">");
				}
				else{
					ErrorCode = COMMAND_OUT_OF_SERVICE;
				}
			}
		}
		printf( "cmd PCI\tE=%d\tch=%u\t%d\t0x%04X\n",
				ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				ValueInDacUnits-OFFSET_IN_DAC_UNITS, ValueInDacUnits );
	}
	else if (strstr(NewCommand, "PC") == NewCommand){ // Program Current command
		float CommandFloatingPointArgument = 22222.2;
		int16_t ValueInDacUnits = 22222; // value in the case of failure (out of range)
		ParsingResult = parseFloatArgument( &CommandFloatingPointArgument, NewCommand+3, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 3+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_PC_INCORRECT_FORMAT;
		}
		else{
			if ((CommandFloatingPointArgument < -COMMAND_FLOATING_POINT_VALUE_LIMIT) ||
					(CommandFloatingPointArgument > COMMAND_FLOATING_POINT_VALUE_LIMIT))
			{
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
					int TemporarySelectedChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
					if (TemporarySelectedChannel < NUMBER_OF_POWER_SUPPLIES){
						UserSetpointDacValue[TemporarySelectedChannel] = (uint16_t)ValueInDacUnits;
						RequiredAmperesValue[TemporarySelectedChannel] = CommandFloatingPointArgument;
					}
					atomic_store_explicit( &OrderCode, ORDER_COMMAND_PC, memory_order_release );
					atomic_store_explicit( &OrderChannel, TemporarySelectedChannel, memory_order_release );
					transmitViaSerialPort(">");
				}
				else{
					ErrorCode = COMMAND_OUT_OF_SERVICE;
				}
			}
		}
		printf( "%12llu\tPC\t%u\tE=%d\t%d\t0x%04X\n",
				time_us_64(),
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				ErrorCode,
				ValueInDacUnits-OFFSET_IN_DAC_UNITS, ValueInDacUnits );
	}
	else if (strstr(NewCommand, "?PCI") == NewCommand){ // "Get set-point value of current" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__PCI_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "%.2f\r\n>",
					RequiredAmperesValue[atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)] );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd ?PCI\tE=%d\tch=%u\t0x%04X\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				UserSetpointDacValue[atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)] );
	}
	else if (strstr(NewCommand, "?PC") == NewCommand){ // "Get set-point value of current" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__PCI_INCORRECT_FORMAT;
		}
		else{
			// essential action
			snprintf( ResponseBuffer, COMMAND_BUFFER_LENGTH-1, "%.2f\r\n>",
					RequiredAmperesValue[atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)] );
			transmitViaSerialPort( ResponseBuffer );
		}
		printf( "cmd ?PC\tE=%d\tch=%u\t0x%04X\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1,
				UserSetpointDacValue[atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)] );
	}
	else if (strstr(NewCommand, "Z") == NewCommand){ // "Select channel" command
		uint8_t TemporaryChannel;
		ParsingResult = parseOneDigitArgument( &TemporaryChannel, NewCommand+1, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 1+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
			ErrorCode = COMMAND_Z_INCORRECT_FORMAT;
		}
		else{
			if ((0 == TemporaryChannel) || (TemporaryChannel > NUMBER_OF_POWER_SUPPLIES)){
				ErrorCode = COMMAND_Z_INCORRECT_VALUE;
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
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND__Z_INCORRECT_FORMAT;
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
	else if (strstr(NewCommand, "PWR") == NewCommand){ // "Switch power on/off" command
		uint8_t TemporaryPowerArgument;
		ParsingResult = parseOneDigitArgument( &TemporaryPowerArgument, NewCommand+5, '\r' );
		if ((ParsingResult < 0) || (CommadLength != 5+ParsingResult+2 ) ||
				(NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n'))
		{
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
		if (IsMainContactorStateOn){
			printf( "cmd pow\tE=%d\tch=%u\tpower on\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
		else{
			printf( "cmd pow\tE=%d\tch=%u\tpower off\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
	}
	else if (strstr(NewCommand, "?PWR") == NewCommand){ // "Get state of power switch" command
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
		if (IsMainContactorStateOn){
			printf( "cmd ?pw\tE=%d\tch=%u\tpower on\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
		else{
			printf( "cmd ?pw\tE=%d\tch=%u\tpower off\n", ErrorCode,
					(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1 );
		}
	}
	else if (strstr(NewCommand, "MC") == NewCommand){ // "Measure current" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_MC_INCORRECT_FORMAT;
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
		printf( "cmd MY\tE=%d\tch=%u\tSig2=%c\n", ErrorCode,
				(unsigned)atomic_load_explicit(&UserSelectedChannel, memory_order_acquire)+1, Sig2Value? '1':'0' );
	}
	else if (strstr(NewCommand, "VERSION") == NewCommand){ // "Get info about the current version" command
		if ((NewCommand[CommadLength-2] != '\r') || (NewCommand[CommadLength-1] != '\n')){
			ErrorCode = COMMAND_VERSION_INCORRECT_FORMAT;
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
	else{
		ErrorCode = COMMAND_UNKNOWN;
		printf( "cmd ???\t" );
		for (int J=0; NewCommand[J] != 0; J++){
			printf( "%c", (NewCommand[J] >= ' ')? NewCommand[J] : '~' );
		}
		printf( "\n" );
	}
	if (COMMAND_GOOD != ErrorCode){
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

static int32_t parseHexadecimal3DigitsArgument( uint16_t *Result, char *TextPtr, char EndMark ){
	uint16_t UInt16_Argument;
	uint8_t CharacterIndex = 0;
	uint8_t Spaces = 0;
	uint8_t DecimalDigits = 0;

	while( CharacterIndex < COMMAND_FLOATING_POINT_MAX_LENGTH ){
		if (EndMark == TextPtr[CharacterIndex]){
			if (0 == DecimalDigits){
				// no digit
				return -6;
			}
			UInt16_Argument = strtoul( &TextPtr[Spaces], NULL, 16 );
			*Result = UInt16_Argument;
			return CharacterIndex;
		}
		else if (' ' == TextPtr[CharacterIndex]){
			Spaces++;
			if (Spaces > 1){
				// too many spaces
				return -5;
			}
			if (DecimalDigits != 0){
				// improper position of space
				return -4;
			}
		}
		else if ((('0' <= TextPtr[CharacterIndex]) && ('9' >= TextPtr[CharacterIndex])) ||
				(('A' <= TextPtr[CharacterIndex]) && ('F' >= TextPtr[CharacterIndex])))
		{
			DecimalDigits++;
			if (DecimalDigits > 3){
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
