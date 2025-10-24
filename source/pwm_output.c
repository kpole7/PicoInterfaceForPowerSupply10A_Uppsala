/// @file pwm_output.c

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "pwm_output.h"

/// @brief The port GPIO6 is used as a PWM output
#define GPIO_FOR_PWM 	6

/// @brief The PWM output frequency is 40kHz
#define PWM_FREQ 		40000

/// @brief This function configures the pin as a pulse generator using hardware PWM
void initializePwm(void){
    gpio_set_function(GPIO_FOR_PWM, GPIO_FUNC_PWM); // Set pin as PWM

    uint SliceNumber = pwm_gpio_to_slice_num(GPIO_FOR_PWM);

    // Set PWM frequency
    uint32_t SystemClock = clock_get_hz(clk_sys); // Usually 125 MHz
    uint32_t Divider = 1;
    uint32_t Wrap = SystemClock / (PWM_FREQ * Divider) - 1;

    pwm_set_clkdiv(SliceNumber, Divider);
    pwm_set_wrap(SliceNumber, Wrap);

    // Set fill factor
    pwm_set_chan_level(SliceNumber, pwm_gpio_to_channel(GPIO_FOR_PWM), Wrap / 8);

    pwm_set_enabled(SliceNumber, true);
}
