/// @file adc_inputs.c

#include <math.h>
#include "hardware/adc.h"
#include "pico/critical_section.h"
#include "adc_inputs.h"


#define ADC_RAW_BUFFER_SIZE 	64
#define GPIO_FOR_ADC0			26
#define GPIO_FOR_ADC1			27


static const float GetVoltageCoefficient = 20.0 / (ADC_RAW_BUFFER_SIZE * 4096.0);
static const float GetVoltageOffset = 10.0;

/// @brief This variable is used in timer interrupt handler
static volatile uint16_t RawBufferAdc0[ADC_RAW_BUFFER_SIZE];

/// @brief This variable is used in timer interrupt handler
static volatile uint16_t RawBufferAdc1[ADC_RAW_BUFFER_SIZE];

/// @brief This variable is used in timer interrupt handler
/// Index for writing new samples from ADC0 and ADC1
static volatile uint32_t AdcBuffersHead = 0;

/// @brief This variable is used in timer interrupt handler
static critical_section_t AdcBuffersCriticalSection;

/// @brief This function initializes peripherals for ADC measuring and the state machine for measurements
void initializeAdcMeasurements(void){
	adc_init();
	adc_gpio_init(GPIO_FOR_ADC0);
	adc_gpio_init(GPIO_FOR_ADC1);

	AdcBuffersHead = 0;
	critical_section_init( &AdcBuffersCriticalSection );
}

/// @brief This function is called by timer interrupt handler
/// This function collects measurements from ADC
void getVoltageSamples(void){
	critical_section_enter_blocking( &AdcBuffersCriticalSection );

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

    critical_section_exit( &AdcBuffersCriticalSection );
}

/// @brief This function measures the voltage at ADC input and make some calculations
///
/// The function acts in the main loop
float getVoltage( uint8_t AdcIndex ){
	if (0 == AdcIndex){

		critical_section_enter_blocking( &AdcBuffersCriticalSection );
		uint32_t Accumulator = 0;
		for (uint8_t J = 0; J < ADC_RAW_BUFFER_SIZE; J++){
			Accumulator += RawBufferAdc0[J];
		}
		critical_section_exit( &AdcBuffersCriticalSection );

		return (float)Accumulator * GetVoltageCoefficient - GetVoltageOffset;
	}
	if (1 == AdcIndex){

		critical_section_enter_blocking( &AdcBuffersCriticalSection );
		uint32_t Accumulator = 0;
		for (uint8_t J = 0; J < ADC_RAW_BUFFER_SIZE; J++){
			Accumulator += RawBufferAdc1[J];
		}
		critical_section_exit( &AdcBuffersCriticalSection );

		return (float)Accumulator * GetVoltageCoefficient - GetVoltageOffset;
	}
	return NAN;
}

