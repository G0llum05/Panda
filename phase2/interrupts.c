#include "headers/interrupts.h"

#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/initial.h"

void interruptHandler() {
    int exccode = getCAUSE() & GETEXECCODE;
    switch (exccode) {
    case IL_TIMER:
        pseudoClockTick();
        break;
    case IL_CPUTIMER:
        localTimerInterrupt();
        break;
    case IL_DISK:
        nonTimerInterrupt(3);
        break;
    case IL_FLASH:
        nonTimerInterrupt(4);
        break;
    case IL_ETHERNET:
        nonTimerInterrupt(5);
        break;
    case IL_PRINTER:
        nonTimerInterrupt(6);
        break;
    case IL_TERMINAL:
        nonTimerInterrupt(7);
        break;
    }
}

static void nonTimerInterrupt(int intLineNo) {
    cpu_t devNo = -1;

    // decreasing priority: 0x10000040 (l3) .. 0x10000040+0x10 (l7)
    cpu_t* devMaskAddr = (cpu_t*)CDEV_BITMAP_ADDR(intLineNo);
    for (cpu_t b = 0; b < N_DEV_PER_IL; b++) {
        if ((*devMaskAddr >> b) & 1) { // NOTE: check right-most first
            devNo = b;
            break;
        }
    }

    if (devNo == -1) // No pending interrupt
        return;

    dtpreg_t* devAddr = (dtpreg_t*)DEV_REG_ADDR(intLineNo, devNo);
    cpu_t status = devAddr->status;
    devAddr->command = ACK; // acknowledge

    if ((int)devAddr < START_DEVREG)
        return; // REVIEW: should pass-up-or-die?

    memaddr index = (memaddr)(devAddr - START_DEVREG) / sizeof(devreg_t);
    pcb_t* proc = removeBlocked((int*)device_semaphores + index); // spec 7.5.4

    if (proc == NULL)
        return;
    proc->p_s.reg_a0 = status;       // spec 7.5.5
    insertProcQ(&ready_queue, proc); // spec 7.5.6
    LDST(&proc->p_s);                // spec 7.5.7
}

static void localTimerInterrupt() {
    // spec 7.2
    setTIMER(TIMESLICE);
    STST(&(running_pcb->p_s));
    insertProcQ(&ready_queue, running_pcb);
    // FIXME: call scheduler()
}

static void pseudoClockTick() {
    LDIT(PSECOND); // Set interval timer spec 7.3.1
    // spec 7.3.2
    memaddr index = (memaddr)49;
    do {
        pcb_t* proc = removeBlocked((int*)device_semaphores + index);
        if (proc)
            insertProcQ(&ready_queue, proc);
    } while (headBlocked((int*)device_semaphores + index));
    // spec 7.3.3
    if (running_pcb != NULL) {
        LDST(&running_pcb->p_s);
    }
}
