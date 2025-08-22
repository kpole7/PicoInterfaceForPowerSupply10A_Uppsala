#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#define PWM_PIN 6
#define PWM_FREQ 40000      // 40kHz

int main() {
    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM); // Ustaw pin jako PWM

    uint slice_num = pwm_gpio_to_slice_num(PWM_PIN);

    // Ustal częstotliwość PWM
    uint32_t sys_clk = clock_get_hz(clk_sys); // Zwykle 125 MHz
    uint32_t divider = 1;
    uint32_t wrap = sys_clk / (PWM_FREQ * divider) - 1;

    pwm_set_clkdiv(slice_num, divider);
    pwm_set_wrap(slice_num, wrap);

    // Ustal współczynnik wypełnienia (np. 50%)
    pwm_set_chan_level(slice_num, pwm_gpio_to_channel(PWM_PIN), wrap / 8);

    pwm_set_enabled(slice_num, true);

    while (true) {
        // Pętla główna jest wolna, PWM działa sprzętowo
        tight_loop_contents();
    }
}

