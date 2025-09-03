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
#include "hardware/irq.h"
#include "hardware/timer.h"

#include "serial_transmission.h"
#include "pwm_output.h"
#include "adc_inputs.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

/// @brief This directive tells that the LED on pico PCB is connected to GPIO25 port
#define PICO_ON_BOARD_LED_PIN		25

/// @brief Period of interrupt: 64 Hz -> 1/64 s ≈ 15625 us
#define TIMER_INTERRUPT_INTERVAL_US	15625


//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes and turns on the LED on pico board.
void turnOnLedOnBoard(void);

/// @brief This is timer interrupt handler for slow cyclic events
int64_t timerInterruptCallback(alarm_id_t id, void *user_data);

//---------------------------------------------------------------------------------------------------
// Main routine
//---------------------------------------------------------------------------------------------------

int main() {
	stdio_init_all();

	serialPortInitialization();
	initializePwm();
	initializeAdcMeasurements();
	initializeDebugDevices();
	turnOnLedOnBoard();

    // Start periodic interrupt
    add_alarm_in_us(TIMER_INTERRUPT_INTERVAL_US, timerInterruptCallback, NULL, true);

	printf("Hello guys %d\n", (int)sizeof(float));

    while (true) {
        // main loop
    	serialPortReceiver();

    	int8_t Temporary = getEventPushButtonChange();
    	if (Temporary != 0){
    		printf("It's me, %d\n", Temporary);

    		if (Temporary < 0){

    			//						12345678901234567890123456789012
    			transmitViaSerialPort( "Abcdefghijklmnopqrstuvwxyz12345." );

    		}
    		changeDebugPin1(true);
    		changeDebugPin1(false);
    	}
    }
}

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void turnOnLedOnBoard(void){
	gpio_init(PICO_ON_BOARD_LED_PIN);
	gpio_set_dir(PICO_ON_BOARD_LED_PIN, GPIO_OUT);
	gpio_put(PICO_ON_BOARD_LED_PIN, true);
}

int64_t timerInterruptCallback(alarm_id_t id, void *user_data){
	getVoltageSamples();

	// timer restart
	return TIMER_INTERRUPT_INTERVAL_US;
}
