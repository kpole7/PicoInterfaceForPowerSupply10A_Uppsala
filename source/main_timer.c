/// @file main_timer.c

#include "pico/stdlib.h"

#include "adc_inputs.h"
#include "psu_talks.h"
#include "main_timer.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#if 0
/// @brief Period of interrupt: 256 Hz -> 1/256 s ≈ 3900 us
#define TIMER_INTERRUPT_INTERVAL_US	3900

#define TIME_DIVIDER_ADC			4	// 256Hz / 4 = 64Hz
#endif


#define TIMER_INTERRUPT_INTERVAL_US	390

#define TIME_DIVIDER_ADC			40


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
	// Analog-to-digital converter operation
	static uint8_t TimeCounterAdc;
	TimeCounterAdc++;
	if (TimeCounterAdc >= TIME_DIVIDER_ADC){
		TimeCounterAdc = 0;
		getVoltageSamples();
	}

	psuTalksTimeTick();

	// timer restart
	return TIMER_INTERRUPT_INTERVAL_US;
}

