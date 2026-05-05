#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

#include "../../headers/types.h"

#define TERM0ADDR 0x10000254
#define TERMSTATMASK 0xFF

// Terminal devices mutex index in device_mutex
#define TERMINALINPUT 8
#define TERMINALOUTPUT 9

void supportExceptionHandler();
void programTrapHandler();
void initializeSupport(support_t* support, unsigned int asid);

#endif
