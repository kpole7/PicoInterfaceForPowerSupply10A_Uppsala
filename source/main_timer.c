/// @file main_timer.c

#include "pico/stdlib.h"

#include "adc_inputs.h"
#include "psu_talks.h"
#include "writing_to_dac.h"
#include "main_timer.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define TIMER_INTERRUPT_INTERVAL_US	600

#define TIME_DIVIDER_ADC			25

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is timer interrupt handler (callback function) for slow cyclic events
/// @callgraph
/// @callergraph
static int64_t timerInterruptHandler(alarm_id_t id, void *user_data);

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes the timer interrupt
void startPeriodicInterrupt(void){
	add_alarm_in_us(TIMER_INTERRUPT_INTERVAL_US, timerInterruptHandler, NULL, true);
}

static int64_t timerInterruptHandler(alarm_id_t id, void *user_data){
//	changeDebugPin1(true);

	// Analog-to-digital converter operation
	static uint8_t TimeCounterAdc;
	TimeCounterAdc++;
	if (TimeCounterAdc >= TIME_DIVIDER_ADC){
		TimeCounterAdc = 0;
		getVoltageSamples();
	}

	writeToDacStateMachine();

//	changeDebugPin1(false);		// measured average frequency 1.66 kHz; max. duration 450 us (2025-12-01)

	// timer restart
	return TIMER_INTERRUPT_INTERVAL_US;
}

