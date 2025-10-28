/// @file psu_talks.c

#include "psu_talks.h"
#include "i2c_outputs.h"
#include "rstl_protocol.h"
#include "debugging.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

/// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2			0x27

/// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1			0x21

#define DAC_NUMBER_OF_BITS			12

#define PSU_ADDRESS_BITS			3

#define GPIO_FOR_NOT_WR_OUTPUT		10

// Output port for switching the power contactor on/off
#define GPIO_FOR_POWER_CONTACTOR	11

#define GPIO_FOR_PSU_LOGIC_FEEDBACK	12

#define I2C_CONSECUTIVE_ERRORS_LIMIT 10

#define DEBUG_DAC					0
#define DEBUG_SAMPLES_DAC			100

//---------------------------------------------------------------------------------------------------
// Local constants
//---------------------------------------------------------------------------------------------------

/// This definition contains a list of states of a finite state machine responsible for programming the DAC of a given PSU
/// The state machine handles communication with two PCF8574 ICs and controls the notWR signal
typedef enum {
	STATE_IDLE,

	STATE_PC_PCX_START,				// setting a new value immediately
	STATE_PC_PCX_1ST_BYTE,
	STATE_PC_PCX_2ND_BYTE,
	STATE_PC_PCX_NOT_WR_SIGNAL,

	STATE_SET_START,				// setting a new value following a ramp
	STATE_SET_1ST_BYTE,
	STATE_SET_2ND_BYTE,
	STATE_SET_NOT_WR_SIGNAL,

	STATE_POWER1_01_Z1,
	STATE_POWER1_02_PCX0,
	STATE_POWER1_03_MY,
	STATE_POWER1_04_PCXFFF,
	STATE_POWER1_05_MY,
	STATE_POWER1_06_PC0,
	STATE_POWER1_07_Z2,
	STATE_POWER1_08_PCX0,
	STATE_POWER1_09_MY,
	STATE_POWER1_10_PCXFFF,
	STATE_POWER1_11_MY,
	STATE_POWER1_12_PC0,
	STATE_POWER1_13_POWER1,

	STATE_ERROR_I2C
}StatesOfPsuFsm;

/// This table shows what needs to be written to the PCF8574 expanders
/// to set a given bit of the digital-to-analog converter (DAC).
static const uint16_t ConvertionDacToPcf8574[DAC_NUMBER_OF_BITS] = {
		0x0080,
		0x0040,
		0x0020,
		0x0010,
		0x0800,
		0x8000,
		0x0100,
		0x0400,
		0x0200,
		0x0002,
		0x0004,
		0x0008
};

// This table shows what needs to be written to the PCF8574 expanders
// to set a given bit of the address of a PSU
static const uint16_t ConvertionPsuAddressToPcf8574[PSU_ADDRESS_BITS] = {
		0x1000,
		0x4000,
		0x2000
};

/// These are physical addresses of the power supply units installed in the equipment
/// The addresses are determined by the dip-switches
static const uint8_t AddressTable[NUMBER_OF_POWER_SUPPLIES] = {
		0,
		1,
		2,
		3
	};

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used in a simple state machine
static volatile StatesOfPsuFsm StateCode;

/// @brief This variable is used to monitor the I2C devices
/// @todo exception handling
static atomic_int I2cConsecutiveErrors;

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

/// @brief This is a computational function.
/// This function does not use variables other than its own.
/// @param DacRawValue binary value (12-bit) to be written to a DAC
/// @param AddressOfPsu hardware address of PSU (determined by the switch SW1)
/// @return 16-bit data to be written to the two PCF8574 integrated circuits
///         the lower byte is to be written to PCF8574 with I2C address PCF8574_ADDRESS_2
///         the higher byte is to be written to PCF8574 with I2C address PCF8574_ADDRESS_1
static uint16_t prepareDataForTwoPcf8574( uint16_t DacRawValue, uint8_t AddressOfPsu );

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

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

/// @brief This function initializes "not WR" output port used to communicate with PSUs
void initializePsuTalks(void){
	gpio_init(GPIO_FOR_NOT_WR_OUTPUT);
	gpio_set_dir(GPIO_FOR_NOT_WR_OUTPUT, GPIO_OUT);
	gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );	// the idle state is high

	StateCode = STATE_IDLE;
	atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );

    gpio_init(GPIO_FOR_POWER_CONTACTOR);
    gpio_put(GPIO_FOR_POWER_CONTACTOR, INITIAL_MAIN_CONTACTOR_STATE);
    gpio_set_dir(GPIO_FOR_POWER_CONTACTOR, true);  // true = output
    gpio_set_drive_strength(GPIO_FOR_POWER_CONTACTOR, GPIO_DRIVE_STRENGTH_12MA);

	gpio_init(GPIO_FOR_PSU_LOGIC_FEEDBACK);
	gpio_set_dir(GPIO_FOR_PSU_LOGIC_FEEDBACK, GPIO_IN);
}

/// @brief This function is called periodically by the time interrupt handler
void psuTalksTimeTick(void){
	static uint16_t WorkingUnsignedArgument;
	bool IsI2cSuccess;

	changeDebugPin1(true);
	changeDebugPin1(false);
	changeDebugPin2(false);

	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_PC){

		changeDebugPin2(true);

		// take a new order
		StateCode = STATE_PC_PCX_START;
		WorkingUnsignedArgument = prepareDataForTwoPcf8574( RequiredDacValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)],
				AddressTable[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] );
		atomic_store_explicit( &OrderCode, ORDER_PROCESSING, memory_order_release );

		return;
	}

	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_COMMAND_SET){

		changeDebugPin2(true);

		// take a new order
		StateCode = STATE_SET_START;
		WorkingUnsignedArgument = prepareDataForTwoPcf8574( RequiredDacValue[atomic_load_explicit(&SelectedChannel, memory_order_acquire)],
				AddressTable[atomic_load_explicit(&SelectedChannel, memory_order_acquire)] );
		atomic_store_explicit( &OrderCode, ORDER_PROCESSING, memory_order_release );

		return;
	}

	if (atomic_load_explicit( &OrderCode, memory_order_acquire ) == ORDER_PROCESSING){

		switch( StateCode ){
		case STATE_PC_PCX_START:
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingUnsignedArgument );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_PC_PCX_1ST_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_PC_PCX_1ST_BYTE:
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingUnsignedArgument >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_PC_PCX_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_PC_PCX_2ND_BYTE:
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			StateCode = STATE_PC_PCX_NOT_WR_SIGNAL;
			break;

		case STATE_PC_PCX_NOT_WR_SIGNAL:
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );

			StateCode = STATE_IDLE;
			atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
			break;

		case STATE_SET_START:
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_2, (uint8_t)WorkingUnsignedArgument );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_SET_1ST_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_SET_1ST_BYTE:
			IsI2cSuccess = i2cWrite( PCF8574_ADDRESS_1, (uint8_t)(WorkingUnsignedArgument >> 8) );
			if (IsI2cSuccess){
				atomic_store_explicit( &I2cConsecutiveErrors, 0, memory_order_release );
				StateCode = STATE_SET_2ND_BYTE;
			}
			else{
				// Exception handling
				if (atomic_load_explicit( &I2cConsecutiveErrors, memory_order_acquire ) < I2C_CONSECUTIVE_ERRORS_LIMIT){
					atomic_fetch_add_explicit( &I2cConsecutiveErrors, 1, memory_order_acq_rel );
				}
				else{
					StateCode = STATE_ERROR_I2C;
					atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
				}
			}
			break;

		case STATE_SET_2ND_BYTE:
			// writing to ADC (signal /WR)
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, false );
			StateCode = STATE_SET_NOT_WR_SIGNAL;
			break;

		case STATE_SET_NOT_WR_SIGNAL:
			gpio_put( GPIO_FOR_NOT_WR_OUTPUT, true );

			StateCode = STATE_IDLE;
			atomic_store_explicit( &OrderCode, ORDER_COMPLETED, memory_order_release );
			break;

		case STATE_ERROR_I2C:

			// todo Exception handling

			StateCode = STATE_IDLE;
			break;

		case STATE_IDLE:
			break;

		default:
		}
	}
}

/// @brief This function changes the power contactor state
void setMainContactorState( bool IsMainContactorStateOn ){
	gpio_put(GPIO_FOR_POWER_CONTACTOR, IsMainContactorStateOn);
}

/// @brief This function reads the logical state of the signal marked as "Sig2" in the diagram
bool getLogicFeedbackFromPsu( void ){
	return gpio_get( GPIO_FOR_PSU_LOGIC_FEEDBACK );
}

