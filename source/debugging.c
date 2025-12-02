/// @file debugging.c

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define GPIO_FOR_PUSH_BUTTON	18
#define CONTACT_FLICKER_TIME	20000

#define GPIO_FOR_DEBUG_PIN_1	15
#define GPIO_FOR_DEBUG_PIN_2	14


uint16_t DebugValueWrittenToPCFs, DebugValueWrittenToDac[NUMBER_OF_POWER_SUPPLIES], DebugCounter1, DebugCounter2;

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializeDebugDevices(void){
	gpio_init(GPIO_FOR_PUSH_BUTTON);
	gpio_set_dir(GPIO_FOR_PUSH_BUTTON, GPIO_IN);
	gpio_pull_up(GPIO_FOR_PUSH_BUTTON);

	gpio_init(GPIO_FOR_DEBUG_PIN_1);
	gpio_set_dir(GPIO_FOR_DEBUG_PIN_1, GPIO_OUT);

	gpio_init(GPIO_FOR_DEBUG_PIN_2);
	gpio_set_dir(GPIO_FOR_DEBUG_PIN_2, GPIO_OUT);

	DebugCounter1 = 0;
	DebugCounter2 = 0;
}

bool getPushButtonState(void){
	return gpio_get(GPIO_FOR_PUSH_BUTTON);
}

/// @brief This function checks if the push button state has changed, ignoring contact flicker
/// @return 0 if the state has not changed
/// @return 1 if the status has changed to "released"
/// @return -1 if the status has changed to "pressed"
int8_t getEventPushButtonChange(void){
	static bool OldStatus = false;
	static uint64_t TimeOfLastChange = 0;

	uint64_t Now = time_us_64();
	if (TimeOfLastChange + CONTACT_FLICKER_TIME > Now){
		return 0; // too quickly
	}

	bool NewStatus = gpio_get(GPIO_FOR_PUSH_BUTTON);
	if (NewStatus == OldStatus){
		return 0; // no change
	}
	OldStatus = NewStatus;
	if (NewStatus){
		return 1;	// status changed to "released"
	}
	return -1;		// status changed to "pressed"
}

void changeDebugPin1( bool NewValue ){
	gpio_put( GPIO_FOR_DEBUG_PIN_1, NewValue );
}

void changeDebugPin2( bool NewValue ){
	gpio_put( GPIO_FOR_DEBUG_PIN_2, NewValue );
}

char* timeTextForDebugging(void){
	static char TimeText[20];
	snprintf( TimeText, sizeof(TimeText)-1, "%12lu", time_us_32() );
	TimeText[10] = 0; // shorten the text
	TimeText[9] = TimeText[8];
	TimeText[8] = TimeText[7];
	TimeText[7] = TimeText[6];
	TimeText[6] = '.';	// insert a period after full seconds
	return TimeText;
}
