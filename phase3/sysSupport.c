#include "headers/sysSupport.h"

#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"

static void supportExceptionHandler() {
    // get cause of support structure
    unsigned int cause = getCAUSE();

    // switch (cause) {
    // // case EXC_MOD ... EXC_UTLBS: pager();
    // // case PROGRAM TRAP: programTrapHandler();
    // // case SYSCALL: supportSyscallHandler();
    // }
}