// psu_talks.c

#include "psu_talks.h"
#include "i2c_outputs.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Directives
//---------------------------------------------------------------------------------------------------

// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2	0x27

// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1	0x21

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

void writeToDac( uint16_t DacValue ){

}

void psuTalksPeriodicIssues(void){
	static uint16_t Counter;
	Counter++;
	if (Counter >= 256){
		Counter = 0;
		i2cWrite( PCF8574_ADDRESS_1, 0 );
	}

	if (1 == Counter){
		i2cWrite( PCF8574_ADDRESS_2, 0xFF );
	}

	changeDebugPin1(false);

	if ((Counter == 64) || (Counter == 192)){
		changeDebugPin2(true);
		bool Result = i2cWrite( PCF8574_ADDRESS_1, 0xFF );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}

	if ((Counter == 65) || (Counter == 193)){
		i2cWrite( PCF8574_ADDRESS_2, 0 );
	}

	if (Counter == 88){
		i2cWrite( PCF8574_ADDRESS_1, 0 );
	}

	if (Counter == 89){
		i2cWrite( PCF8574_ADDRESS_2, 0xFF );
	}
}

