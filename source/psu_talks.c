/// @file psu_talks.c

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "psu_talks.h"
#include "i2c_outputs.h"
#include "rstl_protocol.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define DAC_NUMBER_OF_BITS				12

#define PSU_ADDRESS_BITS				3

#define GPIO_FOR_NOT_WR_OUTPUT			10

// Output port for switching the power contactor on/off
#define GPIO_FOR_POWER_CONTACTOR		11

#define GPIO_FOR_PSU_LOGIC_FEEDBACK		12

#define I2C_CONSECUTIVE_ERRORS_LIMIT	10

/// This constant is used to define the ramp according to which the current changes occur.
/// The output current changes more slowly in this region:
/// OFFSET_IN_DAC_UNITS-NEAR_ZERO_REGION_IN_DAC_UNITS ... OFFSET_IN_DAC_UNITS+NEAR_ZERO_REGION_IN_DAC_UNITS
#define NEAR_ZERO_REGION_IN_DAC_UNITS	15

/// This constant defines the maximum rate of change of current
/// 1 DAC unit = aprox. 0.05% of 10A
#define FAST_RAMP_STEP_IN_DAC_UNITS		30

/// This constant defines the rate of change of current near zero
#define SLOW_RAMP_STEP_IN_DAC_UNITS		1

/// This constant defines the time intervals for the ramp generator
/// For ? intervals ? measured in debug mode (SIMULATE_HARDWARE_PSU == 1) 2025-11-14
#define RAMP_DELAY						202

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
static volatile WritingToDacStates WritingToDacState;

/// @brief This variable is used in a simple state machine
static volatile uint32_t WritingToDacChannel;

/// @brief This variable is used in a simple state machine
static volatile bool WritingToDacIsValidData[NUMBER_OF_POWER_SUPPLIES];

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

/// @brief This function calculates a new setpoint value for the DAC, that corresponds to a single step of the ramp
/// If the ramp crosses zero, it may require additional slowdown.
/// The true zero level is an analog value, so thresholds slightly below zero and slightly above zero are necessary.
/// We never know exactly what digital value corresponds to analog zero (furthermore, a difference should be made between "0+" and "0-").
/// @param ResultDacValue points to a result value that is the DAC setpoint following the ramp towards the TargetValue
/// @param TargetValue user-specified value converted to DAC setting
/// @param PresentValue the last setting written to the DAC
static void calculateRampStep( uint16_t *ResultDacValue, uint16_t TargetValue, uint16_t PresentValue );

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
void initializePsuTalks(void){
	gpio_init(GPIO_FOR_NOT_WR_OUTPUT);
	gpio_set_dir(GPIO_FOR_NOT_WR_OUTPUT, GPIO_OUT);
	gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );	// the idle state is high

	for (uint32_t J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		WritingToDacIsValidData[J] = false;
	}
	WritingToDacState = WRITING_TO_DAC_INITIALIZE;
	atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );

    gpio_init(GPIO_FOR_POWER_CONTACTOR);
    gpio_put(GPIO_FOR_POWER_CONTACTOR, INITIAL_MAIN_CONTACTOR_STATE);
    gpio_set_dir(GPIO_FOR_POWER_CONTACTOR, true);  // true = output
    gpio_set_drive_strength(GPIO_FOR_POWER_CONTACTOR, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(GPIO_FOR_PSU_LOGIC_FEEDBACK);
	gpio_set_dir(GPIO_FOR_PSU_LOGIC_FEEDBACK, GPIO_IN);
}

/// @brief This function provides DAC write support for all power supplies
/// This function drives the state machines of each PSU. Each PSU has its own state machine,
/// which allows them to operate simultaneously. All state machines are identical.
/// This function is called periodically by the time interrupt handler.
void writeToDacStateMachine(void){
	static uint16_t WorkingDataForTwoPcf8574[NUMBER_OF_POWER_SUPPLIES];
	static uint32_t RampDelay[NUMBER_OF_POWER_SUPPLIES];
	bool IsI2cSuccess;

	assert( WritingToDacChannel < NUMBER_OF_POWER_SUPPLIES );
	switch( WritingToDacState ){
	case WRITING_TO_DAC_INITIALIZE:
		gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );
		WritingToDacChannel++;
		if (NUMBER_OF_POWER_SUPPLIES == WritingToDacChannel){
			WritingToDacChannel = 0;
		}

		if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_PC){
			int TemporarySelectedChannel = atomic_load_explicit(&OrderChannel, memory_order_acquire);
			assert( TemporarySelectedChannel < NUMBER_OF_POWER_SUPPLIES );

			WorkingDataForTwoPcf8574[TemporarySelectedChannel] = prepareDataForTwoPcf8574( RequiredDacValue[TemporarySelectedChannel],
					AddressTable[TemporarySelectedChannel] );

			WritingToDacIsValidData[TemporarySelectedChannel] = true;

			atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );

		}

		WritingToDacState = WRITING_TO_DAC_SEND_1ST_BYTE;
		break;

	case WRITING_TO_DAC_SEND_1ST_BYTE:
		if (WritingToDacIsValidData[WritingToDacChannel]){
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingDataForTwoPcf8574[WritingToDacChannel] );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				WritingToDacState = WRITING_TO_DAC_SEND_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					WritingToDacState = WRITING_TO_DAC_FAILURE;
				}
			}
		}
		else{
			WritingToDacState = WRITING_TO_DAC_SEND_2ND_BYTE;
		}
		break;

	case WRITING_TO_DAC_SEND_2ND_BYTE:
		if (WritingToDacIsValidData[WritingToDacChannel]){
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingDataForTwoPcf8574[WritingToDacChannel] >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				WritingToDacState = WRITING_TO_DAC_LATCH_DATA;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					WritingToDacState = WRITING_TO_DAC_FAILURE;
				}
			}
		}
		else{
			WritingToDacState = WRITING_TO_DAC_LATCH_DATA;
		}
		break;

	case WRITING_TO_DAC_LATCH_DATA:
		if (WritingToDacIsValidData[WritingToDacChannel]){
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			WrittenRequiredValue[WritingToDacChannel] = RequiredDacValue[WritingToDacChannel];
			WritingToDacIsValidData[WritingToDacChannel] = false;

			uint32_t DacAddress = decodeDataSentToPcf8574s( &DebugValueWrittenToDac[0], DebugValueWrittenToPCFs );

			changeDebugPin1(true);
			printf( "%12llu  i2c\t%d %d\t%d %d\n", time_us_64(),
					WritingToDacChannel,
					RequiredDacValue[WritingToDacChannel]-OFFSET_FOR_DEBUGGING,
					DacAddress,
					DebugValueWrittenToDac[DacAddress]-OFFSET_FOR_DEBUGGING );
			changeDebugPin1(false);		// measured time = ? us;  2025-10-??

		}
		WritingToDacState = WRITING_TO_DAC_INITIALIZE;
		break;

	case WRITING_TO_DAC_FAILURE:

		// todo Exception handling

		WritingToDacState = WRITING_TO_DAC_INITIALIZE;
		break;

	default:
	}

#if 0
	// debugging
	static WritingToDacStates OldStateCode;

	if (!getPushButtonState()){
		WritingToDacState = WRITING_TO_DAC_SEND_1ST_BYTE;
	}
	if (OldStateCode != WritingToDacState){
		printf( "state %d\n", WritingToDacState );
		OldStateCode = WritingToDacState;
	}
#endif
}

/// @brief This function changes the power contactor state
void setMainContactorState( bool IsMainContactorStateOn ){
	gpio_put(GPIO_FOR_POWER_CONTACTOR, IsMainContactorStateOn);
}

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void ){
	return gpio_get( GPIO_FOR_PSU_LOGIC_FEEDBACK );
}

static void calculateRampStep( uint16_t *ResultDacValue, uint16_t TargetValue, uint16_t PresentValue ){
	uint16_t TemporaryRequiredDacValue = TargetValue;
	uint16_t RampStep = FAST_RAMP_STEP_IN_DAC_UNITS;
	if (PresentValue > (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
		if (TargetValue < (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
			//			           |  <----X----<                   the arrow indicates the present value and the required value
			//			-----------|---0---|----------------> I
			TemporaryRequiredDacValue = (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS);
		}
		else{
			//			           |       |   <--------<
			//			           |       <--------<
			//			           |       |   >-------->
			//			-----------|---0---|----------------> I
			// do not slow down
		}
	}
	else if (PresentValue < (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)){
		if (TargetValue   > (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)){
			//			      >----X---->  |
			//			-----------|---0---|----------------> I
			TemporaryRequiredDacValue = (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS);
		}
		else{
			//			 <------<  |       |
			//			 >------>  |       |
			//			    >------>       |
			//			-----------|---0---|----------------> I
			// do not slow down
		}
	}
	else{
		if ((PresentValue <= (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)) &&
				(TargetValue > (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)))
		{
			//			           |     >-->
			//			           |       >-->
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS;
		}
		else if ((PresentValue >= (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)) &&
				(TargetValue < (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)))
		{
			//			         <--<      |
			//			        <--<       |
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS;
		}
		else{
			//			           | <--<  |
			//			           | >-->  |
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS;
		}
	}

	// calculate a single step of the ramp
	if (TemporaryRequiredDacValue >= PresentValue){
		if (TemporaryRequiredDacValue > PresentValue + RampStep){
			TemporaryRequiredDacValue = PresentValue + RampStep;
		}
	}
	else{
		if (TemporaryRequiredDacValue < PresentValue - RampStep){
			TemporaryRequiredDacValue = PresentValue - RampStep;
		}
	}

	*ResultDacValue = TemporaryRequiredDacValue;
}
