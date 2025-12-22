/// @file compilation_time.c
/// This file should be compiled every time the “make” command is invoked

#include "config.h"

#if SIMULATE_HARDWARE_PSU > 0
#pragma message("Build: SIMULATE_HARDWARE_PSU=1 (simulation)")
#else
#pragma message("Build: SIMULATE_HARDWARE_PSU=0 (hardware)")
#endif


/// This text contains compilation date and time
const char CompilationTime[30] = __DATE__ ", " __TIME__;
