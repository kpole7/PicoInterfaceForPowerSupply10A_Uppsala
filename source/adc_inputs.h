// adc_inputs.h
/// @file adc_inputs.h
/// @brief This module measures input voltages on ADC0 and ADC1

#ifndef SOURCE_ADC_INPUTS_H_
#define SOURCE_ADC_INPUTS_H_

/// @brief This function initializes peripherals for ADC measuring and the state machine for measurements
void initializeAdcMeasurements(void);

/// @brief This function collects measurements from ADC; it is to be called by timer interrupt
void getVoltageSamples(void);


#endif /* SOURCE_ADC_INPUTS_H_ */
