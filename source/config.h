/// @file config.h
/// @brief This header file contains low-level (hardware-related) definitions

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define SIMULATE_HARDWARE_PSU			1

#define OFFSET_FOR_DEBUGGING			2048

#define NUMBER_OF_POWER_SUPPLIES		4

/// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2				0x27

/// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1				0x21

#endif /* SOURCE_CONFIG_H_ */
