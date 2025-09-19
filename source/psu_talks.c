// psu_talks.c

#include "psu_talks.h"
#include "i2c_outputs.h"
#include "rstl_protocol.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2	0x27

// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1	0x21

#define DAC_NUMBER_OF_BITS	12

#define PSU_ADDRESS_BITS	3

//---------------------------------------------------------------------------------------------------
// Constants
//---------------------------------------------------------------------------------------------------

// This definition contains a list of states of a finite state machine responsible for programming the DAC of a given PSU
// The state machine handles communication with two PCF8574 ICs and controls the notWR signal
typedef enum {
	STATE_IDLE,
	STATE_SENDING_1ST_BYTE,
	STATE_SENDING_2ND_BYTE,
}StatesOfPsuFsm;

// This table shows what needs to be written to the PCF8574 expanders
// to set a given bit of the digital-to-analog converter (DAC).
uint16_t ConvertionDacToPcf8574[DAC_NUMBER_OF_BITS] = {
		0x0002,
		0x0200,
		0x0400,
		0x0100,
		0x8000,
		0x0800,
		0x0010,
		0x0020,
		0x0040,
		0x0080,
		0x0008,
		0x0004
};

// This table shows what needs to be written to the PCF8574 expanders
// to set a given bit of the address of a PSU
uint16_t ConvertionPsuAddressToPcf8574[PSU_ADDRESS_BITS] = {
		0x1000,
		0x4000,
		0x2000
};

//---------------------------------------------------------------------------------------------------
// Variables
//---------------------------------------------------------------------------------------------------




//---------------------------------------------------------------------------------------------------
// Local function prototypes
//---------------------------------------------------------------------------------------------------

static uint16_t prepareDataForTwoPcf8574( uint16_t DacRawValue, uint8_t AddressOfPsu );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

// @param DacRawValue binary value (12-bit) to be written to a DAC
// @param AddressOfPsu hardware address of PSU (determined by the switch SW1)
// @return 16-bit data to be written to the two PCF8574 integrated circuits
//         the lower byte is to be written to PCF8574 with I2C address 0x2F
//         the higher byte is to be written to PCF8574 with I2C address 0x21
static uint16_t prepareDataForTwoPcf8574( uint16_t DacRawValue, uint8_t AddressOfPsu ){
	uint16_t Result = 0;
	for (uint8_t J = 0; J < DAC_NUMBER_OF_BITS; J++){
		if ((DacRawValue & (1 << J)) != 0){
			Result |= ConvertionDacToPcf8574[J];
		}
	}
	for (uint8_t J = 0; J < PSU_ADDRESS_BITS; J++){
		if ((AddressOfPsu & (1 << J)) != 0){
			Result |= ConvertionPsuAddressToPcf8574[J];
		}
	}
	return Result;
}

bool writeToDac( uint16_t DacValue ){
	bool I2cSuccess;
	uint16_t DataForTwoPcf8574 = prepareDataForTwoPcf8574( DacValue, SelectedChannel );

	I2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)DataForTwoPcf8574 );

	if (I2cSuccess){
		sleep_us( 500 );
		I2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(DataForTwoPcf8574 >> 8) );
	}

	if (I2cSuccess){
		sleep_us( 50 );

		// writing signal = /WR

		sleep_us( 50 );


	}

	return I2cSuccess;
}

void psuTalksTimeTick(void){
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
		bool Result = i2cWrite( PCF8574_ADDRESS_2, 0 );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}

	if (Counter == 88){
		bool Result = i2cWrite( PCF8574_ADDRESS_1, 0 );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}

	if (Counter == 89){
		bool Result = i2cWrite( PCF8574_ADDRESS_2, 0xFF );
		if (Result){
			changeDebugPin1(true);
		}
		changeDebugPin2(false);
	}
}

