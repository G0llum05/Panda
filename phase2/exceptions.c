#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include "../headers/const.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "uriscv/types.h"

// macro to determine if an exception is an interrupt or not
// works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

// pass up or die sub-handler
static void _puodHandler(int idx) {}

static void _createProcess() {};
static void _termProcess() {};
static void _passeren() {};
static void _verhogen() {};
static void _doIO() {};
static void _getTime() {};
static void _clockWait() {};

static void _getSupportPtr() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = (memaddr) running_pcb->p_supportStruct;
};

// todo: add comments?
static void _getProcessId() {
    if(GET_EXCEPTION_STATE_PTR(0)->reg_a1 == 0) {
        GET_EXCEPTION_STATE_PTR(0)->reg_a0 = running_pcb->p_pid;
    } else {
        if(running_pcb->p_parent == NULL) {
            GET_EXCEPTION_STATE_PTR(0)->reg_a0 = 0;
        } else {
            GET_EXCEPTION_STATE_PTR(0)->reg_a0 = running_pcb->p_parent->p_pid;
        }
    }
};

static void _yield() {};

// syscalls sub-handler
static void _syscallHandler() {

    // get privileges
    int mode = GET_EXCEPTION_STATE_PTR(0)->status & MSTATUS_MPP_MASK;

    // privileged syscall requested
    if(GET_EXCEPTION_STATE_PTR(0)->reg_a0 < 0) {
        if(mode != MSTATUS_MPP_MASK) { // review needed
            // send a trap
            setCAUSE(PRIVINSTR);
            _puodHandler(GENERALEXCEPT);
        } else {

            // increase PC value to avoid infinite syscall loops
            GET_EXCEPTION_STATE_PTR(0)->pc_epc += 4;

            switch(GET_EXCEPTION_STATE_PTR(0)->reg_a0) {
                case(CREATEPROCESS):
                    _createProcess();
                    break;
                case(TERMPROCESS):
                    _termProcess();
                    break;
                case(PASSEREN):
                    _passeren();
                    break;
                case(VERHOGEN):
                    _verhogen();
                    break;
                case(DOIO):
                    _doIO();
                    break;
                case(GETTIME):
                    _getTime();
                    break;
                case(CLOCKWAIT):
                    _clockWait();
                    break;
                case(GETSUPPORTPTR):
                    _getSupportPtr();
                    break;
                case(GETPROCESSID):
                    _getProcessId();
                    break;
                case(YIELD):
                    _yield();
                    break;
            }

            // restore prev state
            LDST(GET_EXCEPTION_STATE_PTR(0));
        }
    }

    // non existent syscall requested, send a trap
    setCAUSE(PRIVINSTR); // review cause, privinstr is not correct
    _puodHandler(GENERALEXCEPT);

}

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