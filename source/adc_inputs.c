/// @file adc_inputs.c

#include <math.h>
#include "hardware/adc.h"
#include "adc_inputs.h"

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define ADC_RAW_BUFFER_SIZE 	64
#define GPIO_FOR_ADC0			26
#define GPIO_FOR_ADC1			27

//---------------------------------------------------------------------------------------------------
// Local constants
//---------------------------------------------------------------------------------------------------

static const float GetVoltageCoefficient = 20.0 / (ADC_RAW_BUFFER_SIZE * 4096.0);
static const float GetVoltageOffset = 10.0;

//---------------------------------------------------------------------------------------------------
// Local variables
//---------------------------------------------------------------------------------------------------

/// @brief This variable is used in timer interrupt handler
static uint16_t RawBufferAdc0[ADC_RAW_BUFFER_SIZE];

/// @brief This variable is used in timer interrupt handler
static uint16_t RawBufferAdc1[ADC_RAW_BUFFER_SIZE];

/// @brief This variable is used in timer interrupt handler
/// Index for writing new samples from ADC0 and ADC1
static volatile uint32_t AdcBuffersHead = 0;

//---------------------------------------------------------------------------------------------------
// Function definitions
//---------------------------------------------------------------------------------------------------

/// @brief This function initializes peripherals for ADC measuring and the state machine for measurements
void initializeAdcMeasurements(void){
	AdcBuffersHead = 0;
	adc_init();
	adc_gpio_init(GPIO_FOR_ADC0);
	adc_gpio_init(GPIO_FOR_ADC1);
}

/// @brief This function collects measurements from ADC; it is to be called by timer interrupt
void getVoltageSamples(void){

	// Measure ADC0
    adc_select_input(0);
    (void)adc_read();                // dummy read
    RawBufferAdc0[AdcBuffersHead] = adc_read();

    // Measure ADC1
    adc_select_input(1);
    (void)adc_read();                // dummy read
    RawBufferAdc1[AdcBuffersHead] = adc_read();

    AdcBuffersHead++;
    if (AdcBuffersHead >= ADC_RAW_BUFFER_SIZE){
    	AdcBuffersHead = 0;
    }
}

/// @brief This function measures the voltage at ADC input and make some calculations
///
/// The function acts in the main loop
float getVoltage( uint8_t AdcIndex ){
	if (0 == AdcIndex){
		uint32_t Accumulator = 0;
		for (uint8_t J = 0; J < ADC_RAW_BUFFER_SIZE; J++){
			Accumulator += RawBufferAdc0[J];
		}
		return (float)Accumulator * GetVoltageCoefficient - GetVoltageOffset;
	}
	if (1 == AdcIndex){
		uint32_t Accumulator = 0;
		for (uint8_t J = 0; J < ADC_RAW_BUFFER_SIZE; J++){
			Accumulator += RawBufferAdc1[J];
		}
		return (float)Accumulator * GetVoltageCoefficient - GetVoltageOffset;
	}
	return NAN;
}

