
#include <assert.h>

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

#define OUT_PIN 	6

#define PERIOD_US 	25
#define TIME_ON		5
#define TIME_OFF	(PERIOD_US-TIME_ON)

#define ALARM_NUM 	0
#define ALARM_IRQ 	TIMER_IRQ_0

volatile uint64_t target;
volatile bool pin_state = false;

// Handler przerwania z hardware timer (alarm)
void timer_irq() {
    gpio_put(OUT_PIN, pin_state);
    // Clear the alarm irq
    hw_clear_bits(&timer_hw->intr, 1u << ALARM_NUM);

    if (pin_state){
        target += TIME_ON;
        timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
        pin_state = false;
    }
    else{
        target += TIME_OFF;
        timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
        pin_state = true;
    }
}

int main() {

	assert( ALARM_IRQ == timer_hardware_alarm_get_irq_num(timer_hw, ALARM_NUM));

    stdio_init_all();
    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_OUT);

    // Enable the interrupt for our alarm (the timer outputs 4 alarm irqs)
    hw_set_bits(&timer_hw->inte, 1u << ALARM_NUM);
    // Set irq handler for alarm irq
    irq_set_exclusive_handler(ALARM_IRQ, timer_irq);

    target = timer_hw->timerawl + PERIOD_US;
    timer_hw->alarm[ALARM_NUM] = (uint32_t) target;
    irq_set_enabled(ALARM_IRQ, true);

    while (true) {

        tight_loop_contents();
    }
}

