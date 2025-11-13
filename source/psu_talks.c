/// @file psu_talks.c

#include <stdio.h>
#include <inttypes.h>
#include "psu_talks.h"
#include "i2c_outputs.h"
#include "rstl_protocol.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

/// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2				0x27

/// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1				0x21

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

#define RAMP_DELAY						20

//---------------------------------------------------------------------------------------------------
// Local constants
//---------------------------------------------------------------------------------------------------

/// This definition contains a list of states of a finite state machine responsible for programming the DAC of a given PSU
/// The state machine handles communication with two PCF8574 ICs and controls the notWR signal
typedef enum {
	STATE_IDLE,

	STATE_PC_PCX_START,				// setting a new value immediately
	STATE_PC_PCX_1ST_BYTE,
	STATE_PC_PCX_2ND_BYTE,
	STATE_PC_PCX_NOT_WR_SIGNAL,

	STATE_SET_START,				// setting a new value following a ramp
	STATE_SET_1ST_BYTE,
	STATE_SET_2ND_BYTE,
	STATE_SET_NOT_WR_SIGNAL,

	STATE_POWER1_TEST_000_START,
	STATE_POWER1_TEST_000_1ST,
	STATE_POWER1_TEST_000_2ND,
	STATE_POWER1_TEST_000_NOT_WR,
	STATE_POWER1_TEST_000_READ,
	STATE_POWER1_TEST_FFF_1ST,
	STATE_POWER1_TEST_FFF_2ND,
	STATE_POWER1_TEST_FFF_NOT_WR,
	STATE_POWER1_TEST_FFF_READ,
	STATE_POWER1_PC0_1ST,
	STATE_POWER1_PC0_2ND,
	STATE_POWER1_PC0_NOT_WR,
	STATE_POWER1_CONTACTOR,

	STATE_ERROR_I2C
}StatesOfPsuFsm;

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
static volatile StatesOfPsuFsm StateCode;

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

/// @brief This function initializes "not WR" output port used to communicate with PSUs
void initializePsuTalks(void){
	gpio_init(GPIO_FOR_NOT_WR_OUTPUT);
	gpio_set_dir(GPIO_FOR_NOT_WR_OUTPUT, GPIO_OUT);
	gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );	// the idle state is high

	StateCode = STATE_IDLE;
	atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );

    gpio_init(GPIO_FOR_POWER_CONTACTOR);
    gpio_put(GPIO_FOR_POWER_CONTACTOR, INITIAL_MAIN_CONTACTOR_STATE);
    gpio_set_dir(GPIO_FOR_POWER_CONTACTOR, true);  // true = output
    gpio_set_drive_strength(GPIO_FOR_POWER_CONTACTOR, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(GPIO_FOR_PSU_LOGIC_FEEDBACK);
	gpio_set_dir(GPIO_FOR_PSU_LOGIC_FEEDBACK, GPIO_IN);
}

/// @brief This function is called periodically by the time interrupt handler
void psuTalksTimeTick(void){
	static uint16_t WorkingDataForTwoPcf8574[NUMBER_OF_POWER_SUPPLIES];
	static uint32_t RampDelay;
	static bool OldSig2;
	bool IsI2cSuccess;
	bool IsExit = false;

	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_PC){
		StateCode = STATE_PC_PCX_START;
		int TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
		WrittenRequiredValue[TemporarySelectedChannel] = RequiredDacValue[TemporarySelectedChannel];
		WorkingDataForTwoPcf8574[TemporarySelectedChannel] = prepareDataForTwoPcf8574( RequiredDacValue[TemporarySelectedChannel],
				AddressTable[TemporarySelectedChannel] );
		atomic_store_explicit( &OrderCode, ORDER_PROCESSING, memory_order_release );

		IsExit = true;
	}

	if (!IsExit && (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_SET)){
		StateCode = STATE_SET_START;
		RampDelay = 0;
		int TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
		uint16_t TemporaryRequiredDacValue;

		calculateRampStep( &TemporaryRequiredDacValue, RequiredDacValue[TemporarySelectedChannel],
				WrittenRequiredValue[TemporarySelectedChannel] );

		WrittenRequiredValue[TemporarySelectedChannel] = TemporaryRequiredDacValue;
		WorkingDataForTwoPcf8574[TemporarySelectedChannel] = prepareDataForTwoPcf8574( TemporaryRequiredDacValue, AddressTable[TemporarySelectedChannel] );
		atomic_store_explicit( &OrderCode, ORDER_PROCESSING, memory_order_release );

		IsExit = true;
	}

	if (!IsExit && (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_POWER_ON)){
		StateCode = STATE_POWER1_TEST_000_START;
		RampDelay = 0;
		int TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
		uint16_t TemporaryRequiredDacValue;

		calculateRampStep( &TemporaryRequiredDacValue, RequiredDacValue[TemporarySelectedChannel],
				WrittenRequiredValue[TemporarySelectedChannel] );

		WrittenRequiredValue[TemporarySelectedChannel] = TemporaryRequiredDacValue;
		WorkingDataForTwoPcf8574[TemporarySelectedChannel] = prepareDataForTwoPcf8574( TemporaryRequiredDacValue, AddressTable[TemporarySelectedChannel] );
		atomic_store_explicit( &OrderCode, ORDER_PROCESSING, memory_order_release );

		IsExit = true;
	}

	if (!IsExit && (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_PROCESSING)){
		int TemporarySelectedChannel;

		switch( StateCode ){
		case STATE_PC_PCX_START:
			TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingDataForTwoPcf8574[TemporarySelectedChannel] );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_PC_PCX_1ST_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_PC_PCX_1ST_BYTE:
			TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingDataForTwoPcf8574[TemporarySelectedChannel] >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_PC_PCX_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_PC_PCX_2ND_BYTE:
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			StateCode = STATE_PC_PCX_NOT_WR_SIGNAL;
			break;

		case STATE_PC_PCX_NOT_WR_SIGNAL:
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );

			changeDebugPin1(true);

			printf( "%12llu  i2c\t%d\n", time_us_64(),
					WrittenRequiredValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)]-OFFSET_FOR_DEBUGGING );

			changeDebugPin1(false);		// measured time = 150 us;  2025-10-30

			StateCode = STATE_IDLE;
			atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
			break;

		case STATE_SET_START:
			if (RampDelay > 0){
				RampDelay--;
			}
			else{
				OldSig2 = getLogicFeedbackFromPsu();

				TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
				IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingDataForTwoPcf8574[TemporarySelectedChannel] );
				if (IsI2cSuccess){
					atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
					StateCode = STATE_SET_1ST_BYTE;
				}
				else{
					// Exception handling
					if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
						atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
					}
					else{
						StateCode = STATE_ERROR_I2C;
						atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
					}
				}
			}
			break;

		case STATE_SET_1ST_BYTE:
			TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingDataForTwoPcf8574[TemporarySelectedChannel] >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_SET_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_SET_2ND_BYTE:
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			StateCode = STATE_SET_NOT_WR_SIGNAL;
			break;

		case STATE_SET_NOT_WR_SIGNAL:
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );

			bool Sig2Value = getLogicFeedbackFromPsu();

			changeDebugPin1(true);

			printf( "%12llu  i2c\t%d\t%c %c\n",
					time_us_64(),
					WrittenRequiredValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)]-OFFSET_FOR_DEBUGGING,
					OldSig2? 'H':'L',
					Sig2Value? 'H':'L' );

			changeDebugPin1(false);		// measured time = 150 us;  2025-10-30

			TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
			if (RequiredDacValue[TemporarySelectedChannel] == WrittenRequiredValue[TemporarySelectedChannel]){
				// The ramp is completed

				StateCode = STATE_IDLE;
				atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
			}
			else{
				// The ramp is continuing

				StateCode = STATE_SET_START;
				TemporarySelectedChannel = atomic_load_explicit(&SelectedChannel, memory_order_acquire);
				uint16_t TemporaryRequiredDacValue;

				calculateRampStep( &TemporaryRequiredDacValue, RequiredDacValue[TemporarySelectedChannel],
						WrittenRequiredValue[TemporarySelectedChannel] );

				WrittenRequiredValue[TemporarySelectedChannel] = TemporaryRequiredDacValue;
				WorkingDataForTwoPcf8574[TemporarySelectedChannel] =
						prepareDataForTwoPcf8574( TemporaryRequiredDacValue, AddressTable[TemporarySelectedChannel] );
				RampDelay = RAMP_DELAY;
			}
			break;

		case STATE_ERROR_I2C:

			// todo Exception handling

			StateCode = STATE_IDLE;
			break;

		case STATE_IDLE:
			break;

		default:
		}
	}

#if 0
	// debugging
	static StatesOfPsuFsm OldStateCode;

	if (!getPushButtonState()){
		StateCode = STATE_IDLE;
	}
	if (OldStateCode != StateCode){
		printf( "state %d\n", StateCode );
		OldStateCode = StateCode;
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
			//			              <----X----<                   the arrow indicates the present value and the required value
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
			//			      >----X---->
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




