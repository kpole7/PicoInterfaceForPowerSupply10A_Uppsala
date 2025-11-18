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

/// @brief User's set-point value for the DAC (number from 0 to 0xFFF)
volatile uint16_t UserSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Setpoint value for the DAC (number from 0 to 0xFFF) at a given moment (follows the ramp)
volatile uint16_t InstantaneousSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Set-point value written to the DAC (number from 0 to 0xFFF)
volatile uint16_t WrittenToDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief This variable is used in a simple state machine
volatile bool WritingToDac_IsValidData[NUMBER_OF_POWER_SUPPLIES];

static uint32_t WritingToDac_RampDelay[NUMBER_OF_POWER_SUPPLIES];

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function calculates a new setpoint value for the DAC, that corresponds to a single step of the ramp
/// If the ramp crosses zero, it may require additional slowdown.
/// The true zero level is an analog value, so thresholds slightly below zero and slightly above zero are necessary.
/// We never know exactly what digital value corresponds to analog zero (furthermore, a difference should be made between "0+" and "0-").
/// @param TargetValue user-specified value converted to DAC setting
/// @param PresentValue the last setting written to the DAC
/// @return the DAC setpoint following the ramp towards the TargetValue
static uint16_t calculateRampStep( uint16_t TargetValue, uint16_t PresentValue );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals.
void initializePsuTalks(void){
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
	return gpio_get( GPIO_FOR_PSU_LOGIC_FEEDBACK );
}

void ordersForDacsAndRampsGeneration( uint32_t Channel ){
	int TemporaryUserSelectedChannel = -1;
	int TemporaryOrderCode = atomic_load_explicit( &OrderCode, memory_order_acquire );
	assert( TemporaryOrderCode < ORDER_COMMAND_ILLEGAL_CODE );
	assert( TemporaryOrderCode >= ORDER_NONE );
	assert( Channel < NUMBER_OF_POWER_SUPPLIES );
	if (TemporaryOrderCode > ORDER_ACCEPTED){
		TemporaryUserSelectedChannel = atomic_load_explicit(&OrderChannel, memory_order_acquire);
		assert( TemporaryUserSelectedChannel < NUMBER_OF_POWER_SUPPLIES );
		// There is a new order for the TemporarySelectedChannel

		if (ORDER_COMMAND_PC == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] = UserSetpointDacValue[TemporaryUserSelectedChannel];
			WritingToDac_IsValidData[TemporaryUserSelectedChannel] = true;
			WritingToDac_RampDelay[Channel] = 0;
		}

		if (ORDER_COMMAND_SET == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] =
					calculateRampStep( UserSetpointDacValue[TemporaryUserSelectedChannel], WrittenToDacValue[TemporaryUserSelectedChannel] );
			WritingToDac_IsValidData[TemporaryUserSelectedChannel] = true;
			WritingToDac_RampDelay[TemporaryUserSelectedChannel] = 0;
		}

		atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
	}

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

	return TemporaryRequiredDacValue;
}
