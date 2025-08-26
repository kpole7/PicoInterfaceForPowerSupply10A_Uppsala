// power_source.c
/// @file power_source.c
/// @brief **Program for RaspberryPi RP2040 acting as an interface for a 10A power supply (Uppsala)**
///
/// ### Details
/// The interface is implemented by the RP2040 processor.
/// The program implements an interface to the power supply consisting of the following elements:
/// * communication with the power supply DAC via I2C,
/// * logic signals (GPIO),
/// * pulse signal generation (PWM),
/// * analog signal measurement (ADC),
/// * reading of logic signals from the power supply (direction of the power supply output current).
/// * On the other hand, the program supports communication with the master unit via UART (RS-232).
/// The communication protocol is implemented by the `serial_transmission.c` module.

#include <stdbool.h>	// just for Eclipse
#include <stdio.h>		// just for debugging

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

#include "serial_transmission.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define PWM_PIN 	6		// GPIO6
#define PWM_FREQ 	40000	// 40kHz

/// @brief This directive tells that the LED on pico PCB is connected to GPIO25 port
#define PICO_ON_BOARD_LED_PIN		25

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes and turns on the LED on pico board.
void turnOnLedOnBoard(void);

/// @brief This function configures the pin as a pulse generator using hardware PWM
void initializePwm(void);

//---------------------------------------------------------------------------------------------------
// Main routine
//---------------------------------------------------------------------------------------------------

int main() {
	stdio_init_all();

	serialPortInitialization();
	initializePwm();
	turnOnLedOnBoard();

	printf("Hello guys\n");

    while (true) {
        // main loop
    	serialPortReceiver();



    }
}

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes and turns on the LED on pico board.
void turnOnLedOnBoard(void){
	gpio_init(PICO_ON_BOARD_LED_PIN);
	gpio_set_dir(PICO_ON_BOARD_LED_PIN, GPIO_OUT);
	gpio_put(PICO_ON_BOARD_LED_PIN, true);
}

/// @brief This function configures the pin as a pulse generator using hardware PWM
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
