#include "headers/initProc.h"
#include "../headers/const.h"
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

    // Initiliaze allocated supports list
    initSupportPool();

    // Setup state
    // NOTE: shell_state is always valid.
    // 1. The test process terminates only when the shell
    // does too, so shell_state is always a valid reference.
    // 2. Even if (1) was not the case, _createProcess() saves
    // the new process state in running_pcb->p_s.
    state_t shell_state;
    setState(&shell_state, SHELLASID);

    // Initialize shell state struct
    support_t* shell_support = allocSupportStruct();
    if (!shell_support)
        PANIC();

    initializeSupport(shell_support, SHELLASID);

    // Create the shell
    SYSCALL(CREATEPROCESS, (int)&shell_state, PROCESS_PRIO_LOW,
            (int)shell_support);

    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
