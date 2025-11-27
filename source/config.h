/// @file config.h
/// @brief This header file contains low-level (hardware-related) definitions

#ifndef SOURCE_CONFIG_H_
#define SOURCE_CONFIG_H_

//---------------------------------------------------------------------------------------------------
// Macro directives
//---------------------------------------------------------------------------------------------------

#define SIMULATE_HARDWARE_PSU			0

// This is a digital value for DAC corresponding to analog zero current
#define OFFSET_IN_DAC_UNITS				2048

#define NUMBER_OF_POWER_SUPPLIES		4

#define PHYSICALLY_INSTALLED_PSU		2

/// Since this interface is a slave, it should not send any messages on its own, but only respond to commands.
/// An exception may be made in the case of critical errors. If this directive has a value of 1,
/// i2c error messages will be sent without prompting.
#define SEND_I2C_ERROR_MESSAGE_ASYNCHRONOUSLY	1

/// The 1'st PCF8574 address (A0=A1=A2=high)
#define PCF8574_ADDRESS_2				0x27

/// The 2'nd PCF8574 address (A0=high; A1=A2=low)
#define PCF8574_ADDRESS_1				0x21

#if SIMULATE_HARDWARE_PSU == 1
#define NUMBER_OF_INSTALLED_PSU			NUMBER_OF_POWER_SUPPLIES
#else
#define NUMBER_OF_INSTALLED_PSU			PHYSICALLY_INSTALLED_PSU
#endif

#endif /* SOURCE_CONFIG_H_ */
