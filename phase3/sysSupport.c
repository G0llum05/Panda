#include "headers/sysSupport.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../testers/h/tconst.h"
#include "headers/initProc.h"
#include "headers/vmSupport.h"
#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"
#include "uriscv/types.h"

static void _supportSyscallHandler();
void programTrapHandler();
static void _terminate();
static void _writeTerminal();
static void _readTerminal();
static void _execute();

void supportExceptionHandler() {
    support_t* support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    unsigned int cause =
        support->sup_exceptState[GENERALEXCEPT].cause & CAUSE_EXCCODE_MASK;

    switch (cause) {
    case EXC_TLBL ... EXC_UTLBS: // never?
        pager();
        break;

    // NOTE: Environment Calls in Machine mode should not be
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

    if (syscall_code > 0 && (mode == MSTATUS_MPP_M)) {
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

static void _terminate() {
    support_t* current_support = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    extern unsigned int master_semaphore;
    // The process should always have a support structure
    if (current_support != NULL) {
        if (current_support->sup_asid == SHELLASID) {
            SYSCALL(VERHOGEN, (memaddr)&master_semaphore, 0, 0);
        } else {
            SYSCALL(VERHOGEN, (memaddr)&shell_mutex, 0, 0);
        }
    }
    SYSCALL(TERMPROCESS, 0, 0, 0);
}

static void _writeTerminal() {
    termreg_t* terminal = (termreg_t*)(TERM0ADDR);
    memaddr* command = (memaddr*)terminal + 3;
    memaddr status;

    support_t* support_structure = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    char* msg = (char*)support_structure->sup_exceptState->reg_a1;
    int char_number = (int)support_structure->sup_exceptState->reg_a2;
    int char_transmitted = 0;

    // Spec 7.2: It is an error to write to a terminal device from an
    // address outside of the requesting U-proc’s logical address space
    if (((memaddr)msg < KUSEG) && (char_number < 0 || char_number > 128))
        SYSCALL(TERMPROCESS, 0, 0, 0);

    trigger_mutex(PASSEREN, TERMINALINPUT);

    while (*msg != EOS) {
        unsigned int value = PRINTCHR | (((unsigned int)*msg) << 8);
        status = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & TERMSTATMASK) != CHARRECV) {
            terminal->transm_status = ~status;
            break;
        }
        char_transmitted++;
        msg++;
    }

    support_structure->sup_exceptState->reg_a0 = char_transmitted;

    trigger_mutex(VERHOGEN, TERMINALINPUT);
}

static void _readTerminal() {
    termreg_t* terminal = (termreg_t*)(TERM0ADDR);
    memaddr* command = (memaddr*)terminal + 1;
    unsigned int status;

    support_t* support_structure = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    char* msg = (char*)support_structure->sup_exceptState->reg_a1;
    int char_transmitted = 0;

    // Spec 7.3: It is an error to write to a terminal device from an
    // address outside of the requesting U-proc’s logical address space
    if ((memaddr)msg < KUSEG) {
        SYSCALL(TERMPROCESS, 0, 0, 0);
    }

    trigger_mutex(PASSEREN, TERMINALOUTPUT);

    while (*msg != EOS) {
        unsigned int value = RECEIVECHAR | (((unsigned int)*msg) << 8);
        status = SYSCALL(DOIO, (int)command, (int)value, 0);
        if ((status & TERMSTATMASK) != CHARRECV) {
            terminal->recv_status = ~status;
            break;
        }
        char_transmitted++;
        msg++;
    }

    support_structure->sup_exceptState->reg_a0 = char_transmitted;

    trigger_mutex(VERHOGEN, TERMINALOUTPUT);
}

void initializeSupport(support_t* support, unsigned int asid) {
    const unsigned int shiftedASID = asid << (ENTRYHI_ASID_BIT);

    // Initialize shell support struct
    context_t* context = support->sup_exceptContext;
    context[GENERALEXCEPT].stackPtr = (memaddr)&support->sup_stackGen[499];
    context[GENERALEXCEPT].status |= MSTATUS_MPP_M;
    context[GENERALEXCEPT].pc = (memaddr)supportExceptionHandler;
    context[PGFAULTEXCEPT].stackPtr = (memaddr)&support->sup_stackTLB[499];
    context[PGFAULTEXCEPT].status |= MSTATUS_MPP_M;
    context[PGFAULTEXCEPT].pc = (memaddr)pager;
    support->sup_asid = asid;

    for (unsigned int i = 0; i < USERPGTBLSIZE; i++) {
        pteEntry_t* entry = &support->sup_privatePgTbl[i];
        // Spec 2.1: if the current page table is the last, then it's
        // the stack and it should have memaddr 0xBFFF.F000
        if (i != USERPGTBLSIZE - 1) {
            entry->pte_entryHI = KUSEG + (i << ENTRYHI_VPN_BIT);
        } else {
            entry->pte_entryHI = (USERSTACKTOP - PAGESIZE) << ENTRYHI_VPN_BIT;
        }
        entry->pte_entryHI |= shiftedASID;
        entry->pte_entryLO |= DIRTYON;
    }
}

static void _execute() {
    unsigned int asid = GET_EXCEPTION_STATE_PTR(0)->reg_a1;
    state_t new_state;
    support_t new_support;

    setState(&new_state, asid);

    initializeSupport(&new_support, asid);

    SYSCALL(CREATEPROCESS, 0, PROCESS_PRIO_HIGH, 0);

    SYSCALL(PASSEREN, (unsigned int)&shell_mutex, 0, 0);

    SYSCALL(VERHOGEN, (unsigned int)&shell_mutex, 0, 0);
}

void programTrapHandler() { SYSCALL(TERMPROCESS, 0, 0, 0); }
