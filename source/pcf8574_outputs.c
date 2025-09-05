// pcf8574_outputs.c


#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "pcf8574_outputs.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define I2C_PORT	i2c0
#define SDA_PIN		4
#define SCL_PIN		5

// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_1	0x27

// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_2	0x21

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializePcf8574Outputs(void){
    i2c_init(I2C_PORT, 50 * 1000); // 50 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
}

bool pcf8574_write( uint8_t I2cAddress, uint8_t Value) {
//	i2c_write_blocking(                I2C_PORT, I2cAddress, &Value, 1, false);
	int Result = i2c_write_timeout_us( I2C_PORT, I2cAddress, &Value, 1, false, 1000 );
	if (1 == Result){
		return true;
	}
	return false;
}

void testPcf8574(void){
	static uint16_t Counter;
	Counter++;
	if (Counter >= 64){
		Counter = 0;
		pcf8574_write( 0x20, 0 );
	}
	if ((Counter == 16) || (Counter == 48)){
		changeDebugPin2(true);
		changeDebugPin1(false);
		bool Result = pcf8574_write( 0x20, 0xFF );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}
	if (Counter == 22){
		pcf8574_write( 0x20, 0 );
	}
}
