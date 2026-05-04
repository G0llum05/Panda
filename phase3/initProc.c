#include "headers/initProc.h"
#include "../headers/const.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"
#include "uriscv/cpu.h"
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

    // Initiliaze allocated supports list
    initSupportPool();

    // Setup state
    const unsigned int shellASID = 3;
    state_t shell_state; // REVIEW(crit): enough lifetime?
    shell_state.reg_sp = USERSTACKTOP;
    shell_state.mie = MIE_ALL;
    shell_state.pc_epc = (memaddr)UPROCSTARTADDR;
    shell_state.status |= MSTATUS_MPIE_MASK;
    shell_state.entry_hi = shellASID << ENTRYHI_ASID_BIT;

    // Initialize shell state struct
    support_t* shell_support = allocSupportStruct();
    if (!shell_support)
        PANIC();

    initiliazeSupport(shell_support, shellASID);

    // Create the shell
    SYSCALL(CREATEPROCESS, (int)&shell_state, PROCESS_PRIO_LOW,
            (int)shell_support);

    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
