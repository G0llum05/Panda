#include "headers/initProc.h"
#include "../headers/const.h"
#include "../phase2/headers/klog.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"
#include "uriscv/liburiscv.h"

void* swap_pool = (void*)FLASHPOOLSTART;
unsigned int shell_mutex = 0;
unsigned int master_semaphore = 0;

// Device mutex
// Flash devices index 0-7
// Terminal output device index 8
// Terminal input device index 9
void trigger_mutex(int code, int index) {
    extern unsigned int device_semaphores[SEMDEVLEN];
    int sem_index = -1;

    // Calcolo dell'indice del semaforo
    if (index < FLASHDEVICES) {
        sem_index = index + 8;
    } else if (index == TERMINALOUTPUT) {
        sem_index = 32;
    } else if (index == TERMINALINPUT) {
        sem_index = 40;
    }
    if (sem_index == -1) {
        klog_print("Capo capo capo! >:( \n"); // good error signaling
        return;
    }
    if (code == PASSEREN || code == VERHOGEN) {
        SYSCALL(code, (int)&device_semaphores[sem_index], 0, 0);
        return;
    }
    klog_print("Hey bôss! >:( \n"); // good error signaling
}

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
    // TODO: check return status
    SYSCALL(CREATEPROCESS, (int)&shell_state, PROCESS_PRIO_LOW,
            (int)shell_support);

    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
