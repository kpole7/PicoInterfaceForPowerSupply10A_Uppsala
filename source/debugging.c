// debugging.c

#include "pico/stdlib.h"
#include "hardware/timer.h"

#include "debugging.h"

#define GPIO_FOR_PUSH_BUTTON	17
#define CONTACT_FLICKER_TIME	20000


void initializeDebugDevices(void){
	gpio_init(GPIO_FOR_PUSH_BUTTON);
	gpio_set_dir(GPIO_FOR_PUSH_BUTTON, GPIO_IN);
	gpio_pull_up(GPIO_FOR_PUSH_BUTTON);
}

bool getPushButtonState(void){
	return gpio_get(GPIO_FOR_PUSH_BUTTON);
}

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




