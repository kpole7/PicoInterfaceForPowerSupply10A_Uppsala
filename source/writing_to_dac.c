/// @file writing_to_dac.c

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "i2c_outputs.h"
#include "writing_to_dac.h"
#include "psu_talks.h"
#include "debugging.h"


//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define DAC_NUMBER_OF_BITS				12

#define PSU_ADDRESS_BITS				3

#define GPIO_FOR_NOT_WR_OUTPUT			10

#define I2C_CONSECUTIVE_ERRORS_LIMIT	10

//---------------------------------------------------------------------------------------------------
// Local constants
//---------------------------------------------------------------------------------------------------

/// This definition contains a list of states of a finite state machine responsible for programming the DAC of a given PSU
/// The state machine handles communication with two PCF8574 ICs and controls the notWR signal
typedef enum {
	WRITING_TO_DAC_INITIALIZE,
	WRITING_TO_DAC_SEND_1ST_BYTE,				// setting a new value immediately
	WRITING_TO_DAC_SEND_2ND_BYTE,
	WRITING_TO_DAC_LATCH_DATA,

	WRITING_TO_DAC_FAILURE
}WritingToDacStates;

/// This table shows what needs to be written to the PCF8574 expanders
/// to set a given bit of the digital-to-analog converter (DAC).
static const uint16_t ConvertionDacToPcf8574[DAC_NUMBER_OF_BITS] = {
		0x0080,
		0x0040,
		0x0020,
		0x0010,
		0x0800,
		0x8000,
		0x0100,
		0x0400,
		0x0200,
		0x0002,
		0x0004,
		0x0008
};

// This table shows what needs to be written to the PCF8574 expanders
// to set a given bit of the address of a PSU
static const uint16_t ConvertionPsuAddressToPcf8574[PSU_ADDRESS_BITS] = {
		0x1000,
		0x4000,
		0x2000
};

/// These are physical addresses of the power supply units installed in the equipment
/// The addresses are determined by the dip-switches
static const uint8_t AddressTable[NUMBER_OF_POWER_SUPPLIES] = {
		0,
		1,
		2,
		3
	};

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used in a simple state machine
static volatile WritingToDacStates WritingToDac_State;

/// @brief This variable is used in a simple state machine
static volatile uint32_t WritingToDac_Channel;

/// @brief This variable is used to monitor the I2C devices
/// @todo exception handling
static atomic_int I2cConsecutiveErrors;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is a computational function.
/// This function does not use variables other than its own.
/// @param DacRawValue binary value (12-bit) to be written to a DAC
/// @param AddressOfPsu hardware address of PSU (determined by the switch SW1)
/// @return 16-bit data to be written to the two PCF8574 integrated circuits
///         the lower byte is to be written to PCF8574 with I2C address PCF8574_ADDRESS_2
///         the higher byte is to be written to PCF8574 with I2C address PCF8574_ADDRESS_1
static uint16_t prepareDataForTwoPcf8574( uint16_t DacRawValue, uint8_t AddressOfPsu );

/// This function is the inverse to the prepareDataForTwoPcf8574 function; this is a debugging tool
static uint32_t decodeDataSentToPcf8574s( uint16_t *DacRawValuePtr, uint16_t Pcf8574Data );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

static uint16_t prepareDataForTwoPcf8574( uint16_t DacRawValue, uint8_t AddressOfPsu ){
	uint16_t Result = 0;
	for (uint8_t J = 0; J < DAC_NUMBER_OF_BITS; J++){
		if ((DacRawValue & (1 << J)) != 0){
			Result |= ConvertionDacToPcf8574[J];
		}
	}
	for (uint8_t J = 0; J < PSU_ADDRESS_BITS; J++){
		if ((AddressOfPsu & (1 << J)) != 0){
			Result |= ConvertionPsuAddressToPcf8574[J];
		}
	}
	return Result;
}

static uint32_t decodeDataSentToPcf8574s( uint16_t *DacRawValuePtr, uint16_t Pcf8574Data ){
	uint32_t AddressOfPsu = 0;
	for (uint8_t J = 0; J < PSU_ADDRESS_BITS; J++){
		if ((Pcf8574Data & ConvertionPsuAddressToPcf8574[J]) != 0){
			AddressOfPsu |= (1 << J);
		}
	}
	if (AddressOfPsu < NUMBER_OF_POWER_SUPPLIES){
		DacRawValuePtr[AddressOfPsu] = 0;
		for (uint8_t J = 0; J < DAC_NUMBER_OF_BITS; J++){
			if ((Pcf8574Data & ConvertionDacToPcf8574[J]) != 0){
				DacRawValuePtr[AddressOfPsu] |= (1 << J);
			}
		}
	}
	return AddressOfPsu;
}

/// @brief This function initializes the module variables and peripherals.
void initializeWritingToDacs(void){
	gpio_init(GPIO_FOR_NOT_WR_OUTPUT);
	gpio_set_dir(GPIO_FOR_NOT_WR_OUTPUT, GPIO_OUT);
	gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );	// the idle state is high

	for (uint32_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		WritingToDac_IsValidData[J] = false;
	}
	WritingToDac_State = WRITING_TO_DAC_INITIALIZE;
	atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
}

/// @brief This function provides DAC write support for all power supplies
/// This function drives the state machines of each PSU. Each PSU has its own state machine,
/// which allows them to operate simultaneously. All state machines are identical.
/// This function is called periodically by the time interrupt handler.
void writeToDacStateMachine(void){
	static uint16_t WorkingDataForTwoPcf8574;
	bool IsI2cSuccess;

	assert( WritingToDac_Channel < NUMBER_OF_POWER_SUPPLIES );
	switch( WritingToDac_State ){
	case WRITING_TO_DAC_INITIALIZE:
		gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );

		psuStateMachine( WritingToDac_Channel );

		WritingToDac_Channel++;
		if (NUMBER_OF_POWER_SUPPLIES == WritingToDac_Channel){
			WritingToDac_Channel = 0;
		}

		if (WritingToDac_IsValidData[WritingToDac_Channel]){
			WorkingDataForTwoPcf8574 = prepareDataForTwoPcf8574( InstantaneousSetpointDacValue[WritingToDac_Channel], AddressTable[WritingToDac_Channel] );
		}

		WritingToDac_State = WRITING_TO_DAC_SEND_1ST_BYTE;
		break;

	case WRITING_TO_DAC_SEND_1ST_BYTE:
		if (WritingToDac_IsValidData[WritingToDac_Channel]){
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingDataForTwoPcf8574 );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				WritingToDac_State = WRITING_TO_DAC_SEND_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					WritingToDac_State = WRITING_TO_DAC_FAILURE;
				}
			}
		}
		else{
			WritingToDac_State = WRITING_TO_DAC_SEND_2ND_BYTE;
		}
		break;

	case WRITING_TO_DAC_SEND_2ND_BYTE:
		if (WritingToDac_IsValidData[WritingToDac_Channel]){
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingDataForTwoPcf8574 >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				WritingToDac_State = WRITING_TO_DAC_LATCH_DATA;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					WritingToDac_State = WRITING_TO_DAC_FAILURE;
				}
			}
		}
		else{
			WritingToDac_State = WRITING_TO_DAC_LATCH_DATA;
		}
		break;

	case WRITING_TO_DAC_LATCH_DATA:
		if (WritingToDac_IsValidData[WritingToDac_Channel]){
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			WrittenToDacValue[WritingToDac_Channel] = InstantaneousSetpointDacValue[WritingToDac_Channel];

			changeDebugPin1(true);
			uint32_t DacAddress = decodeDataSentToPcf8574s( &DebugValueWrittenToDac[0], DebugValueWrittenToPCFs );
			printf( "%12llu\ti2c\t%d\t%d\t%d\t%d\t%d\n",
					time_us_64(),
					WritingToDac_Channel,
					InstantaneousSetpointDacValue[0]-OFFSET_IN_DAC_UNITS,
					InstantaneousSetpointDacValue[1]-OFFSET_IN_DAC_UNITS,
					InstantaneousSetpointDacValue[2]-OFFSET_IN_DAC_UNITS,
					InstantaneousSetpointDacValue[3]-OFFSET_IN_DAC_UNITS );
			if ((WritingToDac_Channel != DacAddress) ||
					(InstantaneousSetpointDacValue[WritingToDac_Channel] != DebugValueWrittenToDac[DacAddress]))
			{
				printf( "\t INCONSISTENCY INCONSISTENCY INCONSISTENCY!!!\n" );
			}
			changeDebugPin1(false);		// measured time = 160 us  (2025-10-19); pulse frequency in the case of ramp execution: 11.4Hz
		}
		WritingToDac_State = WRITING_TO_DAC_INITIALIZE;
		break;

	case WRITING_TO_DAC_FAILURE:

		// todo Exception handling

		WritingToDac_State = WRITING_TO_DAC_INITIALIZE;
		break;

	default:
	}

#if 0
	// debugging
	static WritingToDacStates OldStateCode;

	if (!getPushButtonState()){
		WritingToDac_State = WRITING_TO_DAC_SEND_1ST_BYTE;
	}
	if (OldStateCode != WritingToDac_State){
		printf( "state %d\n", WritingToDac_State );
		OldStateCode = WritingToDac_State;
	}
#endif
}


