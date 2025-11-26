/// @file i2c_outputs.c
/// I2C configuration:
/// Raspberry Pi Pico 2020
/// I2C port GPIO8=SDA, GPIO9=SCL
/// I2C frequency = 50 kHz
/// I2C timeout = 600 us
/// Measured SCL frequency = 47.85 kHz
/// Measured time of 1 byte transmission = 400 us

#include "hardware/i2c.h"

#include "i2c_outputs.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define I2C_PORT		i2c0
#define GPIO_FOR_SDA	8
#define GPIO_FOR_SCL	9

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes I2C port used to communicate with PCF8574
void initializeI2cOutputs(void){
    i2c_init(I2C_PORT, 50 * 1000); // 50 kHz
    gpio_set_function(GPIO_FOR_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GPIO_FOR_SCL, GPIO_FUNC_I2C);
}

/// @brief This function writes one byte of data to the PCF8574 IC.
/// This function does not use variables other than its own.
/// @param I2cAddress the hardware address of one of the two PCF8574 ICs
/// @param Value data to be stored in the PCF8574
/// @return true on success
/// @return false on failure
bool i2cWrite( uint8_t I2cAddress, uint8_t Value) {

	changeDebugPin2(true);

#if SIMULATE_HARDWARE_PSU == 0

	int Result = i2c_write_timeout_us( I2C_PORT, I2cAddress, &Value, 1, false, 600 );

#else
	// debugging

    for (volatile uint32_t DebugCounter = 0; DebugCounter < 5000; DebugCounter++) {
        __asm volatile("nop");
	}
	int Result = 1;

	if ( !getPushButtonState() ){
		Result = 0;
	}
#endif

	changeDebugPin2(false); // measured time = 420 us;  2025-10-30

#if 1 // debugging
	if (PCF8574_ADDRESS_1 == I2cAddress){
		DebugValueWrittenToPCFs &= 0x00FFu;
	}
	if (PCF8574_ADDRESS_2 == I2cAddress){
		DebugValueWrittenToPCFs &= 0xFF00u;
	}
#endif

	if (1 == Result){

#if 1 // debugging
		if (PCF8574_ADDRESS_1 == I2cAddress){
			DebugValueWrittenToPCFs |= ((uint16_t)Value << 8);
		}
		if (PCF8574_ADDRESS_2 == I2cAddress){
			DebugValueWrittenToPCFs |= (uint16_t)Value;
		}
#endif

		return true;
	}
	return false;
}

