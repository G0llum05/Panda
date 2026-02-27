#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include "headers/interrupts.h"
#include "uriscv/types.h"

// macro to determine if an exception is an interrupt or not
// works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

// syscalls sub-handler
static void _syscallHandler() {

    // get privileges
    int mode = GET_EXCEPTION_STATE_PTR(0)->status & MSTATUS_MPP_MASK;

    // privileged syscall requested
    if(GET_EXCEPTION_STATE_PTR(0)->reg_a0 < 0) {

        if(mode != MSTATUS_MPP_MASK) { // review needed

            // syscall not authorized, send a trap

        } else {

            // all good, proceed

        }

    } else {

        // non privileged syscall requested, proceed

    }

}

// pass up or die sub-handler
static void _puodHandler(int idx) {}

// temporary function definition in order to compile initial.c
void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) BIOSDATAPAGE);
}

// Handles all exceptions, exclusive of TLB-Refill events.
void exceptionHandler() {
    unsigned int cause = getCAUSE();

    if(CAUSE_IS_INT(cause)) {
        interruptHandler();
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