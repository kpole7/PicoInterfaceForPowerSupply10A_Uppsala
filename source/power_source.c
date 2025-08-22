#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define PWM_PIN 	6		// GPIO6
#define PWM_FREQ 	40000	// 40kHz

// This function configures the pin as a pulse generator using hardware PWM
void initializePwm(void);

int main() {

	initializePwm();

    while (true) {
        // main loop
        tight_loop_contents();
    }
}

// This function configures the pin as a pulse generator using hardware PWM
void initializePwm(void){
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM); // Set pin as PWM

    uint SliceNumber = pwm_gpio_to_slice_num(PWM_PIN);

    // Set PWM frequency
    uint32_t SystemClock = clock_get_hz(clk_sys); // Usually 125 MHz
    uint32_t Divider = 1;
    uint32_t Wrap = SystemClock / (PWM_FREQ * Divider) - 1;

    pwm_set_clkdiv(SliceNumber, Divider);
    pwm_set_wrap(SliceNumber, Wrap);

    // Set fill factor
    pwm_set_chan_level(SliceNumber, pwm_gpio_to_channel(PWM_PIN), Wrap / 8);

    pwm_set_enabled(SliceNumber, true);
}
