/// @file psu_talks.c

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "psu_talks.h"
#include "rstl_protocol.h"
#include "writing_to_dac.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

// Output port for switching the power contactor on/off
#define GPIO_FOR_POWER_CONTACTOR		11

#define GPIO_FOR_PSU_LOGIC_FEEDBACK		12

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
/// For ? intervals ? measured in debug mode (SIMULATE_HARDWARE_PSU == 1) 2025-11-?
#define RAMP_DELAY						8 // 202

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

atomic_int PsuState;

/// @brief User's set-point value for the DAC (number from 0 to 0xFFF)
volatile uint16_t UserSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Setpoint value for the DAC (number from 0 to 0xFFF) at a given moment (follows the ramp)
volatile uint16_t InstantaneousSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Set-point value written to the DAC (number from 0 to 0xFFF)
volatile uint16_t WrittenToDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief This variable is used in a simple state machine
volatile bool WritingToDac_IsValidData[NUMBER_OF_POWER_SUPPLIES];

/// @brief The state of the power contactor: true=power on; false=power off
bool IsMainContactorStateOn;

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

static uint32_t WritingToDac_RampDelay[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief The function calculates the value to be programmed into the DAC in the next step of the ramp.
/// The current DAC status is represented by PresentValue. The target value set by the user (in DAC units) is given by TargetValue.
/// The digital value to be programmed into the DAC is in the range 0 ... FULL_SCALE_IN_DAC_UNITS.
/// The output current of the power supply is zeroed for a setpoint value approximately equal to OFFSET_IN_DAC_UNITS
/// (you never know exactly what digital value corresponds to analog zero).
/// The maximum rate of change of the setpoint in DAC units is FAST_RAMP_STEP_IN_DAC_UNITS per cycle period.
/// Near zero output current (corresponding to the OFFSET_IN_DAC_UNITS value at the DAC input), there is an area of slower changes.
/// This area extends from -OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS  to  OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS.
/// In this area, the rate of change of the setpoint (in DAC units) is SLOW_RAMP_STEP_IN_DAC_UNITS for the cycle period.
/// @param TargetValue user-specified value (in DAC units)
/// @param PresentValue present value at the DAC input
/// @return setpoint value for DAC in the present ramp step
static uint16_t calculateRampStep( uint16_t TargetValue, uint16_t PresentValue );

static void psuFsmRunning( uint32_t Channel );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializePsuTalks(void){
	atomic_store_explicit( &PsuState, PSU_RUNNING, memory_order_release );
	IsMainContactorStateOn = INITIAL_MAIN_CONTACTOR_STATE;

	initializeWritingToDacs();

    gpio_init(GPIO_FOR_POWER_CONTACTOR);
    gpio_put(GPIO_FOR_POWER_CONTACTOR, INITIAL_MAIN_CONTACTOR_STATE);
    gpio_set_dir(GPIO_FOR_POWER_CONTACTOR, true);  // true = output
    gpio_set_drive_strength(GPIO_FOR_POWER_CONTACTOR, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(GPIO_FOR_PSU_LOGIC_FEEDBACK);
	gpio_set_dir(GPIO_FOR_PSU_LOGIC_FEEDBACK, GPIO_IN);
}

/// @brief This function changes the power contactor state
void setMainContactorState( bool IsMainContactorStateOn ){
	gpio_put(GPIO_FOR_POWER_CONTACTOR, IsMainContactorStateOn);
}

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void ){

#if SIMULATE_HARDWARE_PSU == 1
	int TemporaryChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
	assert(TemporaryChannel < NUMBER_OF_POWER_SUPPLIES);
	bool Result = (WrittenToDacValue[TemporaryChannel] >= OFFSET_IN_DAC_UNITS);
	return Result;

#else
	return gpio_get( GPIO_FOR_PSU_LOGIC_FEEDBACK );
#endif

}

void psuStateMachine( uint32_t Channel ){
	assert( Channel < NUMBER_OF_POWER_SUPPLIES );

	int TemporaryPsuState = atomic_load_explicit( &PsuState, memory_order_acquire );

	switch( TemporaryPsuState ){
	case PSU_STOPPED:

		break;

	case PSU_INITIAL_TEST_SIG2_LOW:

		break;

	case PSU_INITIAL_TEST_SIG2_HIGH:

		break;

	case PSU_INITIAL_ZEROING:

		break;

	case PSU_INITIAL_CONTACTOR_ON:

		break;

	case PSU_RUNNING:
		psuFsmRunning( Channel );
		break;

	case PSU_SHUTTING_DOWN_ZEROING:

		break;

	case PSU_SHUTTING_DOWN_CONTACTOR_OFF:

		break;

	default:

	}
}

static void psuFsmRunning( uint32_t Channel ){
	// orders
	int TemporaryOrderCode = atomic_load_explicit( &OrderCode, memory_order_acquire );
	assert( TemporaryOrderCode < ORDER_COMMAND_ILLEGAL_CODE );
	assert( TemporaryOrderCode >= ORDER_NONE );

	int TemporaryUserSelectedChannel = -1;

	if (TemporaryOrderCode > ORDER_ACCEPTED){
		TemporaryUserSelectedChannel = atomic_load_explicit(&OrderChannel, memory_order_acquire);
		assert( TemporaryUserSelectedChannel >= 0 );
		assert( TemporaryUserSelectedChannel < NUMBER_OF_POWER_SUPPLIES );
		// There is a new order for the TemporarySelectedChannel

		if (ORDER_COMMAND_PCI == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] = UserSetpointDacValue[TemporaryUserSelectedChannel];
			WritingToDac_IsValidData[TemporaryUserSelectedChannel] = true;
			WritingToDac_RampDelay[Channel] = 0;
		}

		if (ORDER_COMMAND_PC == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] =
					calculateRampStep( UserSetpointDacValue[TemporaryUserSelectedChannel], WrittenToDacValue[TemporaryUserSelectedChannel] );
			WritingToDac_IsValidData[TemporaryUserSelectedChannel] = true;
			WritingToDac_RampDelay[TemporaryUserSelectedChannel] = 0;
		}

		atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
	}

	// continuation of ramps
	if (TemporaryUserSelectedChannel != Channel){
		if (WrittenToDacValue[Channel] == UserSetpointDacValue[Channel]){
			// there is nothing to do
			WritingToDac_IsValidData[Channel] = false;
			WritingToDac_RampDelay[Channel] = 0;
		}
		else{
			// The ramp continuation
			if (0 == WritingToDac_RampDelay[Channel]){
				WritingToDac_RampDelay[Channel] = RAMP_DELAY;
			}
			else{
				WritingToDac_RampDelay[Channel]--;
			}

			if (0 == WritingToDac_RampDelay[Channel]){
				// next ramp step
				InstantaneousSetpointDacValue[Channel] =
						calculateRampStep( UserSetpointDacValue[Channel], WrittenToDacValue[Channel] );
				WritingToDac_IsValidData[Channel] = true;
			}
			else{
				// wait
				WritingToDac_IsValidData[Channel] = false;
			}
		}
	}
}

static uint16_t calculateRampStep( uint16_t TargetValue, uint16_t PresentValue ){
	uint16_t TemporaryRequiredDacValue = TargetValue;
	uint16_t RampStep = FAST_RAMP_STEP_IN_DAC_UNITS;

	// Deal with the area near zero current.
	if (PresentValue > (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
		if (TargetValue < (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
			//			           |  <---------<                   the arrow indicates the present value and the user-specified setpoint value
			//			-----------|---0---|----------------> I
			TemporaryRequiredDacValue = (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS);	// The ramp will be fast, but possibly shortened
		}
		else{
			//			           |       |   <--------<
			//			           |       <--------<
			//			           |       |   >-------->
			//			-----------|---0---|----------------> I
			// do nothing
		}
	}
	else if (PresentValue < (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)){
		if (TargetValue   > (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)){
			//			      >--------->  |
			//			-----------|---0---|----------------> I
			TemporaryRequiredDacValue = (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS);	// The ramp will be fast, but possibly shortened
		}
		else{
			//			 <------<  |       |
			//			 >------>  |       |
			//			    >------>       |
			//			-----------|---0---|----------------> I
			// do nothing
		}
	}
	else{
		if ((PresentValue <= (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)) &&
				(TargetValue > (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)))
		{
			//			           |     >-->
			//			           |       >-->
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS; // slow down
		}
		else if ((PresentValue >= (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)) &&
				(TargetValue < (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)))
		{
			//			         <--<      |
			//			        <--<       |
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS; // slow down
		}
		else{
			//			           | <--<  |
			//			           | >-->  |
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS; // slow down
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

	return TemporaryRequiredDacValue;
}
