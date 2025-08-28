// debugging.c

#include "pico/stdlib.h"
#include "debugging.h"

#define GPIO_FOR_PUSH_BUTTON	17

void initializeDebugDevices(void){
	gpio_init(GPIO_FOR_PUSH_BUTTON);
	gpio_set_dir(GPIO_FOR_PUSH_BUTTON, GPIO_IN);
	gpio_pull_up(GPIO_FOR_PUSH_BUTTON);
}

bool getPushButtonState(void){
	return gpio_get(GPIO_FOR_PUSH_BUTTON);
}


