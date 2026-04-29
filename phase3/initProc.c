#include "headers/initProc.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"
#include "uriscv/liburiscv.h"

void* swap_pool = (void*)FLASHPOOLSTART;
unsigned int shell_mutex = 0;
unsigned int master_semaphore = 0;

// Device mutex
// Flash devices index 0-7
// Terminal input device index 8
// Terminal output device index 9
unsigned int device_mutex[FLASHDEVICES + TERMINALDEVICES];

// Instatiator process
void test() {
    // Initialize Swap Pool Table
    initSwapStructs();

    // Initialize shell state struct
    state_t shell_state;
    STST(&shell_state);
    shell_state.reg_sp = USERSTACKTOP;
    shell_state.pc_epc = KUSEG;
    shell_state.status |= MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    shell_state.mie = MIE_ALL;

    // Initialize shell support struct
    support_t shell_support;
    shell_support.sup_exceptContext[GENERALEXCEPT].stackPtr =
        (memaddr)USERSTACKTOP;
    shell_support.sup_exceptContext[GENERALEXCEPT].status |=
        MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    shell_support.sup_exceptContext[GENERALEXCEPT].pc =
        (memaddr)supportExceptionHandler;
    shell_support.sup_exceptContext[PGFAULTEXCEPT].stackPtr =
        (memaddr)USERSTACKTOP;
    shell_support.sup_exceptContext[PGFAULTEXCEPT].status |=
        MSTATUS_MPIE_MASK | MSTATUS_MPP_U;
    shell_support.sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;

    // Create the shell
    SYSCALL(CREATEPROCESS, (int)&shell_state, PROCESS_PRIO_LOW,
            (int)&shell_support);

    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
