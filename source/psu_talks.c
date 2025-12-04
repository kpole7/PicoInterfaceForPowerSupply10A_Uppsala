/// @file psu_talks.c

#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include "psu_talks.h"
#include "rstl_protocol.h"
#include "writing_to_dac.h"
#include "debugging.h"

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
/// 1 DAC unit = aprox. 0.05% of 10A (5 mA)
#define FAST_RAMP_STEP_IN_DAC_UNITS		30

/// This constant defines the rate of change of current near zero
#define SLOW_RAMP_STEP_IN_DAC_UNITS		1

/// This constant defines the time intervals for the ramp generator
/// For RAMP_DELAY==8, one step of the ramp takes 88 ms (11.4 Hz)
#define RAMP_DELAY						8

#define ANALOG_SIGNALS_STABILIZATION		480
#define ANALOG_SIGNALS_LONG_STABILIZATION	(2*ANALOG_SIGNALS_STABILIZATION)

//---------------------------------------------------------------------------------------------------
// Global variables
//---------------------------------------------------------------------------------------------------

/// FSM state; takes values from PsuOperatingStates
atomic_uint_fast16_t PsuState;

/// @brief User's set-point value for the DAC (number from 0 to 0xFFF)
atomic_uint_fast16_t UserSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Setpoint value for the DAC (number from 0 to 0xFFF) at a given moment (follows the ramp)
uint16_t InstantaneousSetpointDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief Set-point value written to the DAC (number from 0 to 0xFFF)
uint16_t WrittenToDacValue[NUMBER_OF_POWER_SUPPLIES];

/// @brief This variable is used in a simple state machine
bool WriteToDacDataReady[NUMBER_OF_POWER_SUPPLIES];

/// @brief The state of the power contactor: true=power on; false=power off
atomic_bool IsMainContactorStateOn;

/// This array is used to store readings of Sig2 for each channel and
/// for two DAC values: 0 and FULL_SCALE_IN_DAC_UNITS; additionally, a flag is used to indicate that the data is valid
atomic_bool Sig2LastReadings[NUMBER_OF_POWER_SUPPLIES][SIG2_RECORD_SIZE];

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

// This variable is used only in the timer interrupt
static uint32_t RampStepDelay[NUMBER_OF_POWER_SUPPLIES];

static uint32_t TransitionalDelay;

static uint16_t FsmChannel;

static bool IsInitialCall;

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

static void psuFsmStopped(void);
static void psuFsmSig2LowSetDac(void);
static void psuFsmSig2LowTest(void);
static void psuFsmSig2HighSetDac(void);
static void psuFsmSig2HighTest(void);
static void psuFsmZeroing(void);
static void psuFsmTurnContactorOn(void);
static void psuFsmRunning(void);
static void psuFsmShutingDownZeroing(void);
static void psuFsmShutingDownSwitchOff(void);

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the module variables and peripherals
void initializePsuTalks(void){
	atomic_store_explicit( &PsuState, PSU_STOPPED, memory_order_release );
	atomic_store_explicit( &IsMainContactorStateOn, false, memory_order_release );
	for (int J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++ ){

		atomic_store_explicit( &Sig2LastReadings[J][SIG2_FOR_0_DAC_SETTING],          false, memory_order_release );	// anything, but defined
		atomic_store_explicit( &Sig2LastReadings[J][SIG2_FOR_FULL_SCALE_DAC_SETTING], false, memory_order_release );
		atomic_store_explicit( &Sig2LastReadings[J][SIG2_IS_VALID_INFORMATION],       false, memory_order_release );
	}

	initializeWritingToDacs();

	IsInitialCall = true;

    gpio_init(GPIO_FOR_POWER_CONTACTOR);
    gpio_put(GPIO_FOR_POWER_CONTACTOR, false);
    gpio_set_dir(GPIO_FOR_POWER_CONTACTOR, true);  // true = output
    gpio_set_drive_strength(GPIO_FOR_POWER_CONTACTOR, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(GPIO_FOR_PSU_LOGIC_FEEDBACK);
	gpio_set_dir(GPIO_FOR_PSU_LOGIC_FEEDBACK, GPIO_IN);
}

/// @brief This function changes the power contactor state
void setMainContactorState( bool NewState ){
	gpio_put(GPIO_FOR_POWER_CONTACTOR, NewState);
}

/// @brief This function reads the logical state of the signal Sig2
bool getLogicFeedbackFromPsu( void ){

#if SIMULATE_HARDWARE_PSU == 1
	uint16_t TemporaryChannel = atomic_load_explicit(&UserSelectedChannel, memory_order_acquire);
	assert(TemporaryChannel < NUMBER_OF_POWER_SUPPLIES);
	bool Result = (WrittenToDacValue[TemporaryChannel] >= OFFSET_IN_DAC_UNITS);
	return Result;

#else
	return gpio_get( GPIO_FOR_PSU_LOGIC_FEEDBACK );
#endif

}

/// @brief This function drives the higher-level state machine
/// The function is called by the lower-level state machine in write_do_dac.c, which is called by
/// the timer interrupt handler; the FSM state is stored in the PsuState variable and takes values
/// from PsuOperatingStates.
/// @return index of power supply channel that should be handled by the lower-level state machine
uint16_t psuStateMachine(void){
	int TemporaryPsuState = atomic_load_explicit( &PsuState, memory_order_acquire );
	assert( TemporaryPsuState < PSU_ILLEGAL_STATE );

	switch( TemporaryPsuState ){
	case PSU_STOPPED:
		psuFsmStopped();
		break;

	case PSU_INITIAL_SIG2_LOW_SET_DAC:
		psuFsmSig2LowSetDac();
		break;

	case PSU_INITIAL_SIG2_LOW_TEST:
		psuFsmSig2LowTest();
		break;

	case PSU_INITIAL_SIG2_HIGH_SET_DAC:
		psuFsmSig2HighSetDac();
		break;

	case PSU_INITIAL_SIG2_HIGH_TEST:
		psuFsmSig2HighTest();
		break;

	case PSU_INITIAL_ZEROING:
		psuFsmZeroing();
		break;

	case PSU_INITIAL_CONTACTOR_ON:
		psuFsmTurnContactorOn();
		break;

	case PSU_RUNNING:
		psuFsmRunning();
		break;

	case PSU_SHUTTING_DOWN_ZEROING:
		psuFsmShutingDownZeroing();
		break;

	case PSU_SHUTTING_DOWN_CONTACTOR_OFF:
		psuFsmShutingDownSwitchOff();
		break;

	default:

	}

#if 1
	static int OldPsuState;
	TemporaryPsuState = atomic_load_explicit( &PsuState, memory_order_acquire );
	if (TemporaryPsuState != OldPsuState){
		printf( "%s\tstate %d -> %d\n",
				timeTextForDebugging(),
				OldPsuState,
				TemporaryPsuState );
		OldPsuState = TemporaryPsuState;
	}
#endif
	return FsmChannel;
}

static void psuFsmStopped(void){
	assert( false == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( false == PhysicalValue );
	(void)PhysicalValue; // So that the compiler doesn't complain

	if (IsInitialCall){
		IsInitialCall = false;

		for (int J=0; J < NUMBER_OF_POWER_SUPPLIES; J++ ){
			WriteToDacDataReady[J] = false;
		}
		FsmChannel = 0;
	}

	// orders
	uint16_t TemporaryOrderCode = atomic_load_explicit( &OrderCode, memory_order_acquire );
	assert( TemporaryOrderCode < ORDER_COMMAND_ILLEGAL_CODE );
	assert( TemporaryOrderCode >= ORDER_NONE );

	if (TemporaryOrderCode > ORDER_ACCEPTED){
		if (ORDER_COMMAND_POWER_UP == TemporaryOrderCode){
			atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
			atomic_store_explicit( &PsuState, PSU_INITIAL_SIG2_LOW_SET_DAC, memory_order_release );
			IsInitialCall = true;
		}
	}
}

static void psuFsmSig2LowSetDac(void){
	assert( false == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( false == PhysicalValue );
	(void)PhysicalValue; // So that the compiler doesn't complain

	if (IsInitialCall){
		IsInitialCall = false;
		FsmChannel = 0;
	}
	else{
		FsmChannel++;
	}

	if (FsmChannel < NUMBER_OF_INSTALLED_PSU){
		// preparatory activities for switching on the contactor:  zeroing DACs
		InstantaneousSetpointDacValue[FsmChannel] = 0;
		WriteToDacDataReady[FsmChannel] = true;
		RampStepDelay[FsmChannel] = 0;
	}
	else{
		for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
			WriteToDacDataReady[J] = false;
		}
		atomic_store_explicit( &PsuState, PSU_INITIAL_SIG2_LOW_TEST, memory_order_release );
		IsInitialCall = true;
		TransitionalDelay = ANALOG_SIGNALS_STABILIZATION;
		FsmChannel = 0;
	}
}

static void psuFsmSig2LowTest(void){
	if (0 != TransitionalDelay){
		TransitionalDelay--;
	}
	else{
		if (IsInitialCall){
			IsInitialCall = false;
			FsmChannel = 0;
		}
		else{
			FsmChannel++;
		}
		if (FsmChannel < NUMBER_OF_INSTALLED_PSU){
			// write down the same, in order to update the Sig2 reading
			WriteToDacDataReady[FsmChannel] = true;
		}
		else{
			for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
				WriteToDacDataReady[J] = false;
			}
			atomic_store_explicit( &PsuState, PSU_INITIAL_SIG2_HIGH_SET_DAC, memory_order_release );
			IsInitialCall = true;
			FsmChannel = 0;
		}
	}
}

static void psuFsmSig2HighSetDac(void){
	assert( false == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( false == PhysicalValue );
	(void)PhysicalValue; // So that the compiler doesn't complain

	if (IsInitialCall){
		IsInitialCall = false;
		FsmChannel = 0;
	}
	else{
		FsmChannel++;
	}
	// continue preparatory activities for switching on the contactor:  set DACs to the maximum
	if (FsmChannel < NUMBER_OF_INSTALLED_PSU){
		InstantaneousSetpointDacValue[FsmChannel] = FULL_SCALE_IN_DAC_UNITS;
		WriteToDacDataReady[FsmChannel] = true;
		RampStepDelay[FsmChannel] = 0;
	}
	else{
		for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
			WriteToDacDataReady[J] = false;
		}
		atomic_store_explicit( &PsuState, PSU_INITIAL_SIG2_HIGH_TEST, memory_order_release );
		TransitionalDelay = ANALOG_SIGNALS_STABILIZATION;
		IsInitialCall = true;
		FsmChannel = 0;
	}
}

static void psuFsmSig2HighTest(void){
	if (0 != TransitionalDelay){
		TransitionalDelay--;
	}
	else{
		if (IsInitialCall){
			IsInitialCall = false;
			FsmChannel = 0;
		}
		else{
			FsmChannel++;
		}
		if (FsmChannel < NUMBER_OF_INSTALLED_PSU){
			// write down the same, in order to update the Sig2 reading
			WriteToDacDataReady[FsmChannel] = true;
		}
		else{
			for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
				WriteToDacDataReady[J] = false;
			}
			atomic_store_explicit( &PsuState, PSU_INITIAL_ZEROING, memory_order_release );
			IsInitialCall = true;
			FsmChannel = 0;
		}
	}
}

static void psuFsmZeroing(void){
	// zeroing before switching on the contactor
	assert( false == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( false == PhysicalValue );
	(void)PhysicalValue; // So that the compiler doesn't complain

	if (IsInitialCall){
		IsInitialCall = false;
		FsmChannel = 0;
	}
	else{
		FsmChannel++;
	}
	// continue preparatory activities for switching on the contactor:  set DACs to the maximum
	if (FsmChannel < NUMBER_OF_INSTALLED_PSU){
		InstantaneousSetpointDacValue[FsmChannel] = OFFSET_IN_DAC_UNITS;
		WriteToDacDataReady[FsmChannel] = true;
		RampStepDelay[FsmChannel] = 0;
	}
	else{
		for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
			WriteToDacDataReady[J] = false;
		}
		atomic_store_explicit( &PsuState, PSU_INITIAL_CONTACTOR_ON, memory_order_release );
		TransitionalDelay = ANALOG_SIGNALS_LONG_STABILIZATION;
		IsInitialCall = true;
		FsmChannel = 0;
	}
}

static void psuFsmTurnContactorOn(void){
	if (0 != TransitionalDelay){
		TransitionalDelay--;
	}
	else{
		assert( false == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
		bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
		assert( false == PhysicalValue );
		(void)PhysicalValue; // So that the compiler doesn't complain

		printf( "Sig2LastReadings:%s\n", convertSig2TableToText());

		for (int J=0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
			if (OFFSET_IN_DAC_UNITS != WrittenToDacValue[J]){
				// something went wrong
				for (int J=0; J < NUMBER_OF_POWER_SUPPLIES; J++ ){
					WriteToDacDataReady[J] = false;
				}
				atomic_store_explicit( &PsuState, PSU_STOPPED, memory_order_release );
				IsInitialCall = true;
				printf("\nInternal error in %s at line %d\n", __FILE__, __LINE__);
				return;
			}
		}
		atomic_store_explicit( &IsMainContactorStateOn, true, memory_order_release );
		setMainContactorState( true );

		printf( "%s\tmain contactor switched on\n",
				timeTextForDebugging());

		atomic_store_explicit( &PsuState, PSU_RUNNING, memory_order_release );
		IsInitialCall = true;
		FsmChannel = 0;
	}
}

static void psuFsmRunning(void){
	assert( true == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( true == PhysicalValue );
	(void)PhysicalValue;  // So that the compiler doesn't complain

	FsmChannel++;
	if (NUMBER_OF_POWER_SUPPLIES <= FsmChannel){
		FsmChannel = 0;
	}

	// continuation of ramps
	if (WrittenToDacValue[FsmChannel] == atomic_load_explicit( &UserSetpointDacValue[FsmChannel], memory_order_acquire )){
		// there is nothing to do
		WriteToDacDataReady[FsmChannel] = false;
		RampStepDelay[FsmChannel] = 0;
	}
	else{
		// the step continuation
		if (0 != RampStepDelay[FsmChannel]){
			RampStepDelay[FsmChannel]--;
			WriteToDacDataReady[FsmChannel] = false;
		}
		else{
			// next ramp step
			InstantaneousSetpointDacValue[FsmChannel] =
					calculateRampStep( atomic_load_explicit( &UserSetpointDacValue[FsmChannel], memory_order_acquire ),
							WrittenToDacValue[FsmChannel] );
			WriteToDacDataReady[FsmChannel] = true;
			RampStepDelay[FsmChannel] = RAMP_DELAY;
		}
	}

	// orders
	uint16_t TemporaryOrderCode = atomic_load_explicit( &OrderCode, memory_order_acquire );
	assert( TemporaryOrderCode < ORDER_COMMAND_ILLEGAL_CODE );
	assert( TemporaryOrderCode >= ORDER_NONE );

	int TemporaryUserSelectedChannel = -1;

	if (TemporaryOrderCode > ORDER_ACCEPTED){
		TemporaryUserSelectedChannel = atomic_load_explicit(&OrderChannel, memory_order_acquire);
		assert( TemporaryUserSelectedChannel >= 0 );
		assert( TemporaryUserSelectedChannel < NUMBER_OF_POWER_SUPPLIES );
		// There is a new order

		if (ORDER_COMMAND_PCI == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] = atomic_load_explicit( &UserSetpointDacValue[TemporaryUserSelectedChannel], memory_order_acquire );
			RampStepDelay[TemporaryUserSelectedChannel] = RAMP_DELAY;
			atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
		}

		if (ORDER_COMMAND_PC == TemporaryOrderCode){
			InstantaneousSetpointDacValue[TemporaryUserSelectedChannel] =
					calculateRampStep( atomic_load_explicit( &UserSetpointDacValue[TemporaryUserSelectedChannel], memory_order_acquire ),
							WrittenToDacValue[TemporaryUserSelectedChannel] );
			RampStepDelay[TemporaryUserSelectedChannel] = RAMP_DELAY;
			atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
		}

		if (ORDER_COMMAND_POWER_DOWN == TemporaryOrderCode){
			for (int J = 0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
				atomic_store_explicit( &UserSetpointDacValue[J], OFFSET_IN_DAC_UNITS, memory_order_release );
				InstantaneousSetpointDacValue[J] = calculateRampStep( OFFSET_IN_DAC_UNITS, WrittenToDacValue[J] );
				RampStepDelay[J] = RAMP_DELAY;
			}
			atomic_store_explicit( &OrderCode, ORDER_ACCEPTED, memory_order_release );
			atomic_store_explicit( &PsuState, PSU_SHUTTING_DOWN_ZEROING, memory_order_release );
			IsInitialCall = true;
			FsmChannel = 0;
		}
	}
}

static void psuFsmShutingDownZeroing(void){
	assert( true == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
	bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
	assert( true == PhysicalValue );
	(void)PhysicalValue;  // So that the compiler doesn't complain

	// continuation of ramps, if not all of them have ended

	FsmChannel++;
	if (NUMBER_OF_POWER_SUPPLIES <= FsmChannel){
		FsmChannel = 0;
	}

	if (OFFSET_IN_DAC_UNITS == WrittenToDacValue[FsmChannel]){
		WriteToDacDataReady[FsmChannel] = false;
		RampStepDelay[FsmChannel] = 0;

		for (int J = 0; J < NUMBER_OF_INSTALLED_PSU; J++ ){
			if (OFFSET_IN_DAC_UNITS != WrittenToDacValue[J]){
				// other channel is continuing its ramp
				return;
			}
			else{
				WriteToDacDataReady[J] = false;
			}
		}
		// all ramps are completed
		atomic_store_explicit( &PsuState, PSU_SHUTTING_DOWN_CONTACTOR_OFF, memory_order_release );
		IsInitialCall = true;
		TransitionalDelay = ANALOG_SIGNALS_LONG_STABILIZATION;
		FsmChannel = 0;
		return;
	}
	else{
		// the step continuation
		if (0 != RampStepDelay[FsmChannel]){
			RampStepDelay[FsmChannel]--;
			WriteToDacDataReady[FsmChannel] = false;
		}
		else{
			// next ramp step
			InstantaneousSetpointDacValue[FsmChannel] =
					calculateRampStep( OFFSET_IN_DAC_UNITS, WrittenToDacValue[FsmChannel] );
			WriteToDacDataReady[FsmChannel] = true;
			RampStepDelay[FsmChannel] = RAMP_DELAY;
		}
	}
}

static void psuFsmShutingDownSwitchOff(void){
	if (0 != TransitionalDelay){
		TransitionalDelay--;
	}
	else{
		assert( true == atomic_load_explicit( &IsMainContactorStateOn, memory_order_acquire ));
		bool PhysicalValue = gpio_get(GPIO_FOR_POWER_CONTACTOR);
		assert( true == PhysicalValue );
		(void)PhysicalValue;  // So that the compiler doesn't complain

		atomic_store_explicit( &IsMainContactorStateOn, false, memory_order_release );
		setMainContactorState( false );

		printf( "%s\tmain contactor switched off\n",
				timeTextForDebugging());

		atomic_store_explicit( &PsuState, PSU_STOPPED, memory_order_release );
		IsInitialCall = true;
	}
}

static uint16_t calculateRampStep( uint16_t TargetValue, uint16_t PresentValue ){
	uint16_t TemporaryRequiredDacValue = TargetValue;
	uint16_t RampStep = FAST_RAMP_STEP_IN_DAC_UNITS;

	// Deal with the area near zero current.
	if (PresentValue > (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
		if (TargetValue < (OFFSET_IN_DAC_UNITS + NEAR_ZERO_REGION_IN_DAC_UNITS)){
			//			        <---------------<                   the arrow indicates the present value and the user-specified setpoint value
			//					   |     <------<
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
			//				  >--------------->
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
			//			           |     >----->
			//			           |       >--->
			//			-----------|---0---|----------------> I
			RampStep = SLOW_RAMP_STEP_IN_DAC_UNITS; // slow down
		}
		else if ((PresentValue >= (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)) &&
				(TargetValue < (OFFSET_IN_DAC_UNITS - NEAR_ZERO_REGION_IN_DAC_UNITS)))
		{
			//			      <------<     |
			//			      <----<       |
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

/// This function prepares information on Sig2 readings in text form
char* convertSig2TableToText(void){
	static char Sig2Table[3*NUMBER_OF_POWER_SUPPLIES+5];
	for (int J = 0; J < NUMBER_OF_POWER_SUPPLIES; J++){
		Sig2Table[3*J]   = ' ';
		if (J >= NUMBER_OF_INSTALLED_PSU){
			Sig2Table[3*J+1] = '-';
			Sig2Table[3*J+2] = '-';
		}
		else{
			if (!atomic_load_explicit( &Sig2LastReadings[J][SIG2_IS_VALID_INFORMATION], memory_order_acquire )){
				Sig2Table[3*J+1] = '?';
				Sig2Table[3*J+2] = '?';
			}
			else{
				Sig2Table[3*J+1] = atomic_load_explicit( &Sig2LastReadings[J][0], memory_order_acquire ) ? 'H' : 'L';
				Sig2Table[3*J+2] = atomic_load_explicit( &Sig2LastReadings[J][1], memory_order_acquire ) ? 'H' : 'L';
			}
		}
	}
	Sig2Table[3*NUMBER_OF_POWER_SUPPLIES]   = 0; // termination character
	return Sig2Table;
}
