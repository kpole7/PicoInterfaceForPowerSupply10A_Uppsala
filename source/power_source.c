#include "pico/stdlib.h"
#include "hardware/timer.h"

#define OUT_PIN 6
#define PERIOD_US 25

// Flaga stanu wyjścia (można też po prostu toggle)
volatile bool pin_state = false;

// Funkcja obsługi przerwania timerowego
bool repeating_timer_callback(struct repeating_timer *t) {
    pin_state = !pin_state;
    gpio_put(OUT_PIN, pin_state);
    // Zwracamy true, aby timer był powtarzany
    return true;
}

int main() {
    stdio_init_all();
    gpio_init(OUT_PIN);
    gpio_set_dir(OUT_PIN, GPIO_OUT);

    struct repeating_timer timer;
    // Uruchamiamy timer hardware, okres 25us
    add_repeating_timer_us(PERIOD_US, repeating_timer_callback, NULL, &timer);

    // Pętla główna do innych zadań
    while (true) {
        // Tu możesz robić dowolne inne rzeczy
        tight_loop_contents();
    }
}

