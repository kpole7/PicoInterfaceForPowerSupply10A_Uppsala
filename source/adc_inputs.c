// adc_inputs.c

#include "pico/stdlib.h"
#include "hardware/adc.h"


#define ADC_RAW_BUFFER_SIZE 	256
#define GPIO_FOR_ADC0			26
#define GPIO_FOR_ADC1			27

static uint16_t RawBufferAdc0[ADC_RAW_BUFFER_SIZE];
static uint16_t RawBufferAdc1[ADC_RAW_BUFFER_SIZE];

/// @brief Index for writing new samples from ADC0 and ADC1
static volatile uint32_t AdcBuffersHead = 0;

void initializeAdcMeasurements(void){
	adc_init();
	adc_gpio_init(GPIO_FOR_ADC0);
	adc_gpio_init(GPIO_FOR_ADC1);

	AdcBuffersHead = 0;
}

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
