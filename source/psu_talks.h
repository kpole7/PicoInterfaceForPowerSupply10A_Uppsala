// psu_talks.h

#ifndef SOURCE_PSU_TALKS_H_
#define SOURCE_PSU_TALKS_H_

#include "pico/stdlib.h"

//---------------------------------------------------------------------------------------------------
// Function prototypes
//---------------------------------------------------------------------------------------------------

void writeToDac( uint16_t DacValue );

/// @brief This function is a debugging tool, normally not used
void psuTalksPeriodicIssues(void);


#endif // SOURCE_PSU_TALKS_H_
