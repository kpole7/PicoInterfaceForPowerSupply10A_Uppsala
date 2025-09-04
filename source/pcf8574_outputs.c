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

// The 1'st PCF8574 address (A0=A1=A2=GND)
#define PCF8574_ADDRESS_1	0x20

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void initializePcf8574Outputs(void){
    i2c_init(I2C_PORT, 50 * 1000); // 50 kHz
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
}

void pcf8574_write(uint8_t Value) {
    i2c_write_blocking(I2C_PORT, PCF8574_ADDRESS_1, &Value, 1, false);
}

void testPcf8574(void){
	static uint16_t Counter;
	Counter++;
	if (Counter >= 32){
		Counter = 0;
		pcf8574_write( 0 );
	}
	if (Counter == 16){
		changeDebugPin2(true);
		pcf8574_write( 0xFF );
		changeDebugPin2(false);
	}
}
