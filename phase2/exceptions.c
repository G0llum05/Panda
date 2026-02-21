#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include "../headers/const.h"

// macro to determine if an exception is an interrupt or not
// works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

// syscalls sub-handler
static void _syscallHandler() {}

// pass up or die sub-handler
static void _puodHandler(int idx) {}

// temporary function to be replaced as soon as the real
// interrupthandler is ready in interrupts.c
static void _interruptHandler() {}

// Handles all exceptions, exclusive of TLB-Refill events.
void exceptionHandler() {
    unsigned int cause = getCAUSE();

    if(CAUSE_IS_INT(cause)) {
        _interruptHandler();
    } else
    switch(CAUSE_CODE(cause)) {

        // syscalls
        case(8):
        case(11):
            _syscallHandler();
            break;
        
        // puod - tlb
        case(24):
        case(25):
        case(26):
        case(27):
        case(28):
            _puodHandler(PGFAULTEXCEPT);
            break;
        
        // puod - traps
        default:
            _puodHandler(GENERALEXCEPT);
            break;
        
    }
}