// main_timer.c

#include "pico/stdlib.h"

#include "adc_inputs.h"
#include "i2c_outputs.h"
#include "main_timer.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

/// @brief Period of interrupt: 64 Hz -> 1/64 s ≈ 15625 us
#define TIMER_INTERRUPT_INTERVAL_US	15625

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is timer interrupt handler for slow cyclic events
int64_t timerInterruptCallback(alarm_id_t id, void *user_data);

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void startPeriodicInterrupt(void){
	add_alarm_in_us(TIMER_INTERRUPT_INTERVAL_US, timerInterruptCallback, NULL, true);
}

int64_t timerInterruptCallback(alarm_id_t id, void *user_data){
	getVoltageSamples();

	testPcf8574();

	// timer restart
	return TIMER_INTERRUPT_INTERVAL_US;
}

