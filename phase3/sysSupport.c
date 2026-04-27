#include "headers/sysSupport.h"
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

static void _supportSyscallHandler() {}

static void _programTrapHandler() {}