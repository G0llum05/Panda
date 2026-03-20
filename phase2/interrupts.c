#include "headers/interrupts.h"

#include <uriscv/arch.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/klog.h"
#include "headers/scheduler.h"
#include "headers/shared.h"

static void nonTimerInterrupt(int intlineNo);
static void localTimerInterrupt();
static void pseudoClockTick();

void interruptHandler() {
#ifdef DEBUG
    klog_print("interruptHandler\n");
#endif
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
#ifdef DEBUG
    klog_print("nonTimerInterrupt\n");
#endif
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
        _exit();

    cpu_t final_status = 0;
    pcb_t* proc = NULL;

    if (intLineNo == 7) { // 7 = terminal IL
        termreg_t* termAddr = (termreg_t*)DEV_REG_ADDR(intLineNo, devNo);
        int base_sem_index = 32 + devNo; // for terminals: +0 output +8 input

        // Transmitter is higher priority than receiver
        if (termAddr->transm_status == OKCHARTRANS) {
            final_status = termAddr->transm_status;
            termAddr->transm_command = ACK;
            proc = removeBlocked((int*)&device_semaphores[base_sem_index + 8]);
        } else { // receiver
            final_status = termAddr->recv_status;
            termAddr->recv_command = ACK; // Spegniamo la tastiera
            proc = removeBlocked((int*)&device_semaphores[base_sem_index]);
        }
    } else { // Disks, Flash, Printers
        dtpreg_t* devAddr = (dtpreg_t*)DEV_REG_ADDR(intLineNo, devNo);
        final_status = devAddr->status;
        devAddr->command = ACK;
        int sem_index = (intLineNo - 3) * DEVPERINT + devNo;
        proc = removeBlocked((int*)&device_semaphores[sem_index]);
    }

    if (proc) {
        proc->p_s.reg_a0 = final_status; // 7.1.5
        soft_block_count--;
        insertProcQ(&ready_queue, proc); // 7.1.6
    }

    _exit(); // 7.1.7
}

static void localTimerInterrupt() {
#ifdef DEBUG
    klog_print("localTimerInterrupt\n");
#endif
    // spec 7.2
    setTIMER(TIMESLICE);
    state_t* old_state = GET_EXCEPTION_STATE_PTR(0);
    _copyState(&running_pcb->p_s, old_state);
    insertProcQ(&ready_queue, running_pcb);
    running_pcb = NULL;
    scheduler();
}

static void pseudoClockTick() {
#ifdef DEBUG
    klog_print("pseudoClockTick\n");
#endif
    LDIT(PSECOND); // Set interval timer spec 7.3.1
    // spec 7.3.2
    int* key = device_semaphores + 49;

    do {
        pcb_t* proc = removeBlocked(key);
        soft_block_count--;
        if (proc)
            insertProcQ(&ready_queue, proc);
    } while (headBlocked(key));

    // spec 7.3.3
    _exit();
}
