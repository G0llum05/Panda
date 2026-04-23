#include "headers/interrupts.h"

#include <uriscv/arch.h>
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "uriscv/const.h"

#define TERMSTATMASK 0xFF

static void _nonTimerInterrupt(int intlineNo);
static void _localTimerInterrupt();
static void _pseudoClockTick();
static void _enqueue(pcb_t* proc);
static void _exit();

void interruptHandler() {
    int exccode = getCAUSE() & CAUSE_EXCCODE_MASK;
    switch (exccode) {
    case IL_TIMER:
        _pseudoClockTick();
        break;
    case IL_CPUTIMER:
        _localTimerInterrupt();
        break;
    default:
        _nonTimerInterrupt(exccode);
        break;
    }
}

static void _nonTimerInterrupt(int intLineNo) {
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

    if (intLineNo == IL_TERMINAL) {
        termreg_t* termAddr = (termreg_t*)DEV_REG_ADDR(intLineNo, devNo);
        int base_sem_index = 32 + devNo; // for terminals: +0 output +8 input

        // Transmitter is higher priority than receiver
        if ((termAddr->transm_status & TERMSTATMASK) == OKCHARTRANS) {
            final_status = termAddr->transm_status;
            termAddr->transm_command = ACK;
            proc = removeBlocked((int*)&device_semaphores[base_sem_index]);
        } else { // receiver
            final_status = termAddr->recv_status;
            termAddr->recv_command = ACK; // Spegniamo la tastiera
            proc = removeBlocked((int*)&device_semaphores[base_sem_index + 8]);
        }
    } else { // Disks, Flash, Printers
        dtpreg_t* devAddr = (dtpreg_t*)DEV_REG_ADDR(intLineNo, devNo);
        final_status = devAddr->status;
        devAddr->command = ACK;
        int sem_index = (intLineNo - DEV_IL_START) * DEVPERINT + devNo;
        proc = removeBlocked((int*)&device_semaphores[sem_index]);
    }

    if (proc) {
        proc->p_s.reg_a0 = final_status; // 7.1.5
        soft_block_count--;
        _enqueue(proc); // 7.1.6
    }

    _exit(); // 7.1.7
}

static void _localTimerInterrupt() {
    // spec 7.2
    setTIMER(TIMESLICE * (*(cpu_t*)TIMESCALEADDR));
    if (running_pcb != NULL) {
        insertProcQ(&ready_queue, running_pcb); // don't use enqueue see spec
    }
    scheduler();
}

static void _pseudoClockTick() {
    LDIT(PSECOND); // Set interval timer spec 7.3.1
    // spec 7.3.2
    int* key = &pseudo_clock_semaphore;

    while (headBlocked(key)) {
        pcb_t* proc = removeBlocked(key);
        if (proc) {
            soft_block_count--;
            insertProcQ(&ready_queue, proc);
        }
    }

    // spec 7.3.3
    _exit();
}

// Prevent process to be enqueued twice in ready_queue
static void _enqueue(pcb_t* proc) {
    if (running_pcb == proc)
        return;
    else
        insertProcQ(&ready_queue, proc); // 7.1.6
}

// Procedure to load state or call the scheduler
static inline void _exit() {
    if (running_pcb) {
        STCK(start_time);
        LDST(&running_pcb->p_s);
    } else
        scheduler();
}
