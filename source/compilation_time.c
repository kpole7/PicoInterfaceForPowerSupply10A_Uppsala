/// @file compilation_time.c
/// This file should be compiled every time the “make” command is invoked

/// This text contains compilation date and time
const char CompilationTime[30] = __DATE__ ", " __TIME__;
