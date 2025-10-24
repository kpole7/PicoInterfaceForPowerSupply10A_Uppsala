/// @file debugging.h
/// @brief This module provides some auxiliaries for debugging


#ifndef SOURCE_DEBUGGING_H_
#define SOURCE_DEBUGGING_H_

#include <stdbool.h>

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

void initializeDebugDevices(void);

bool getPushButtonState(void);

/// @brief This function checks if the push button state has changed, ignoring contact flicker
/// @return 0 if the state has not changed
/// @return 1 if the status has changed to "released"
/// @return -1 if the status has changed to "pressed"
int8_t getEventPushButtonChange(void);

void changeDebugPin1( bool NewValue );

void changeDebugPin2( bool NewValue );

#endif // SOURCE_DEBUGGING_H_
