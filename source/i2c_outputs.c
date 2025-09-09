// i2c_outputs.c

#include "hardware/i2c.h"

#include "i2c_outputs.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define I2C_PORT	i2c0
#define SDA_PIN		4
#define SCL_PIN		5

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializeI2cOutputs(void){
    i2c_init(I2C_PORT, 50 * 1000); // 50 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
}

bool i2cWrite( uint8_t I2cAddress, uint8_t Value) {
	int Result = i2c_write_timeout_us( I2C_PORT, I2cAddress, &Value, 1, false, 1000 ); // Timeout 1000us for PCF8574 working with I2C at 50kHz
	if (1 == Result){
		return true;
	}
	return false;
}

