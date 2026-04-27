#include "headers/initProc.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "uriscv/liburiscv.h"

void* swap_pool = (void*)FLASHPOOLSTART;
unsigned int shell_mutex = 0;
unsigned int master_semaphore = 0;

// Sem I/O
unsigned int device_mutex[FLASHDEVICES + TERMINALDEVICES];

// Instatiator process
void test() {
    // Initialize Swap Pool Table

    // Initialize shell support struct

    // Create the shell (incomplete)
    SYSCALL(CREATEPROCESS, 0, PROCESS_PRIO_HIGH, 0);

    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
