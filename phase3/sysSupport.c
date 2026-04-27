#include "headers/sysSupport.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "headers/vmSupport.h"
#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"

static void _supportSyscallHandler();
static void _programTrapHandler();

void supportExceptionHandler() {
    // REVIEW: does this function automatically get the
    // cause from the correct exception state?
    // It is important, otherwise we don't know where
    // to route the exception in the switch.
    unsigned int cause = getCAUSE();

    switch (cause) {
    case EXC_TLBL ... EXC_UTLBS:
        pager();
        break;

    // REVIEW: Environment Calls in Machine mode should not be
    // processed by the support level exception handler
    case EXC_ECU:
        _supportSyscallHandler();
        break;

    // Spec 4.2: "[...] TLB-modification exceptions should not occur.
    // If they do, they should be treated as a program trap."
    default:
        _programTrapHandler();
        break;
    }
}

static void _supportSyscallHandler() {
    support_t* process_support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Store the Machine Previous Privilege mode
    int mode = process_support->sup_exceptState[GENERALEXCEPT].status;

    const int syscall_code = exc_state->reg_a0;

    // Increase PC value and go to next instruction
    exc_state->pc_epc += 4;
}

static void _programTrapHandler() { SYSCALL(TERMPROCESS, 0, 0, 0); }