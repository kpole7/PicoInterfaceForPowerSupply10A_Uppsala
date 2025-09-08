// i2c_outputs.c

#include "pico/stdlib.h"
#include "hardware/i2c.h"

#include "i2c_outputs.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

#define I2C_PORT	i2c0
#define SDA_PIN		4
#define SCL_PIN		5

// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2	0x27

// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1	0x21

//---------------------------------------------------------------------------------------------------
// Local function prototypes
//---------------------------------------------------------------------------------------------------

static inline bool i2cWrite( uint8_t I2cAddress, uint8_t Value);

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializeI2cOutputs(void){
    i2c_init(I2C_PORT, 50 * 1000); // 50 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
}

void testPcf8574(void){
	static uint16_t Counter;
	Counter++;
	if (Counter >= 64){
		Counter = 0;
		i2cWrite( PCF8574_ADDRESS_1, 0 );
	}

	if (1 == Counter){
		i2cWrite( PCF8574_ADDRESS_2, 0xFF );
	}

	changeDebugPin1(false);

	if ((Counter == 16) || (Counter == 48)){
		changeDebugPin2(true);
		bool Result = i2cWrite( PCF8574_ADDRESS_1, 0xFF );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}

	if ((Counter == 17) || (Counter == 49)){
		i2cWrite( PCF8574_ADDRESS_2, 0 );
	}

	if (Counter == 22){
		i2cWrite( PCF8574_ADDRESS_1, 0 );
	}

	if (Counter == 23){
		i2cWrite( PCF8574_ADDRESS_2, 0xFF );
	}
}

void writeToDac( uint16_t DacValue ){

}


static inline bool i2cWrite( uint8_t I2cAddress, uint8_t Value) {
	int Result = i2c_write_timeout_us( I2C_PORT, I2cAddress, &Value, 1, false, 1000 ); // Timeout 1000us for PCF8574 working with I2C at 50kHz
	if (1 == Result){
		return true;
	}
	return false;
}

