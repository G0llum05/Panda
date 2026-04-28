#include "headers/sysSupport.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../testers/h/tconst.h"
#include "headers/vmSupport.h"
#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"

static void _supportSyscallHandler();
void programTrapHandler();
static void _terminate();
static void _writeTerminal();
static void _readTerminal();
static void _execute();

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
        programTrapHandler();
        break;
    }
}

static void _supportSyscallHandler() {
    support_t* process_support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Store the Machine Previous Privilege mode
    int mode = process_support->sup_exceptState[GENERALEXCEPT].status;

    const int syscall_code =
        process_support->sup_exceptState[GENERALEXCEPT].reg_a0;

    // Increase PC value and go to next instruction
    process_support->sup_exceptState[GENERALEXCEPT].pc_epc += 4;

    if (syscall_code > 0 && (mode == MSTATUS_MPP_U)) {
        switch (syscall_code) {
        case (TERMINATE):
            _terminate();
            break;
        case (WRITETERMINAL):
            _writeTerminal();
            break;
        case (READTERMINAL):
            _readTerminal();
            break;
        case (EXECUTE):
            _execute();
            break;
        default:
            programTrapHandler();
            break;
        }
        LDST(&process_support->sup_exceptState[GENERALEXCEPT]);
    }
    programTrapHandler();
}

static void _terminate() { SYSCALL(TERMPROCESS, 0, 0, 0); }

static void _writeTerminal() {}

static void _readTerminal() {}

static void _execute() {}

void programTrapHandler() { SYSCALL(TERMPROCESS, 0, 0, 0); }
