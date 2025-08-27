// power_source.c
/// @file power_source.c
/// @brief **Program for the RaspberryPi RP2040-based interface for a 10A power supply (Uppsala)**
///
/// ### Details
///
/// The interface is implemented on the RP2040 processor. The program performs the following functions:
/// #### 1.   Communication with 8 power supplies via I2C and logic signals (GPIO).
/// - Addressing power supplies (I2C).
/// - Controlling digital-to-analog converters (I2C and GPIO).
/// - Reading logic signals from 8 power supplies representing the direction of the power supplies' output currents (GPIO).
/// #### 2.   Generating a pulse signal (PWM).
/// #### 3.   Analog measurement of signals from 8 power supplies.
/// - Channel multiplexing (GPIO).
/// - Analog voltage measurement (ADC0).
/// #### 4.   Communication with the main unit via a serial port (UART0).
/// The communication protocol is implemented in the `serial_transmission.c` module.


#include <stdbool.h>	// just for Eclipse
#include <stdio.h>		// just for debugging

#include "pico/stdlib.h"

#include "serial_transmission.h"
#include "pwm_output.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

/// @brief This directive tells that the LED on pico PCB is connected to GPIO25 port
#define PICO_ON_BOARD_LED_PIN		25

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes and turns on the LED on pico board.
void turnOnLedOnBoard(void);

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

