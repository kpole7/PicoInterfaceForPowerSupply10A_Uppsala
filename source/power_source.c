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
/// The communication protocol is implemented in the `uart_talks.c` module.
///
/// Abbreviations: PSU = power source unit;  FSM = finite state machine

#include <stdbool.h>	// just for Eclipse
#include <stdio.h>		// just for debugging

#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "hardware/timer.h"

#include "uart_talks.h"
#include "pwm_output.h"
#include "psu_talks.h"
#include "adc_inputs.h"
#include "i2c_outputs.h"
#include "main_timer.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

/// @brief This directive tells that the LED on pico PCB is connected to GPIO25 port
#define GPIO_FOR_PICO_ON_BOARD_LED	25

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes and turns on the LED on pico board.
static void turnOnLedOnBoard(void);

//---------------------------------------------------------------------------------------------------
// Main routine
//---------------------------------------------------------------------------------------------------

int main() {
	stdio_init_all();

	serialPortInitialization();
	initializePwm();
	initializeI2cOutputs();
	initializePsuTalks();
	initializeAdcMeasurements();
	initializeDebugDevices();
	initializeRstlProtocol();
	turnOnLedOnBoard();

    for (volatile uint32_t DebugCounter = 0; DebugCounter < 5000; DebugCounter++) {
        __asm volatile("nop");
	}
	startPeriodicInterrupt();

	printf("Hello guys\n");
	if (SIMULATE_HARDWARE_PSU == 1){
		printf("simulation mode\n");
	}

    while (true) {
        // main loop
    	driveUserInterface();

#if 0 // debugging
    	int8_t Temporary = getEventPushButtonChange();
    	if (Temporary != 0){
    		printf("It's me, %d\n", Temporary);
    		if (Temporary < 0){
    			//						12345678901234567890123456789012
    			transmitViaSerialPort( "Abcdefghijklmnopqrstuvwxyz12345." );
    		}
    	}
#endif

    }
}

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

static void turnOnLedOnBoard(void){
	gpio_init(GPIO_FOR_PICO_ON_BOARD_LED);
	gpio_set_dir(GPIO_FOR_PICO_ON_BOARD_LED, GPIO_OUT);
	gpio_put(GPIO_FOR_PICO_ON_BOARD_LED, true);
}

