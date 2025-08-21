
#include <assert.h>

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

#define OUT_PIN 		6
#define DEBUG_PIN 		7

#define PERIOD_US 		25
#define PULSE_TIME_ON	3

#define ALARM_NUM 		0
#define ALARM_IRQ 		TIMER_IRQ_0

volatile uint64_t target;

// Handler przerwania z hardware timer (alarm)
void timer_irq() {
	gpio_put(OUT_PIN, true);

	// Clear the alarm irq
	hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);
	target += PERIOD_US;
	timer_hw->alarm[ALARM_NUM] = (uint32_t) target;

	sleep_us( PULSE_TIME_ON );
	gpio_put(OUT_PIN, false);
}

int main() {

	assert( ALARM_IRQ == timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM));

    stdio_init_all();

    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_OUT);

    gpio_init(DEBUG_PIN);
    gpio_set_dir(DEBUG_PIN, GPIO_OUT);

    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(ALARM_IRQ, timer_irq);

    target = timer_hw->timerawl + PERIOD_US;
    timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
    irq_set_enabled(ALARM_IRQ, true);

    while (true) {
    	gpio_put(DEBUG_PIN, true);
    	gpio_put(DEBUG_PIN, false);
    }
}

