#include "headers/scheduler.h"

#include "uriscv/liburiscv.h"
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../phase1/headers/pcb.h"

#include "headers/initial.h"
#include "headers/klog.h"

void scheduler() {
#ifdef DEBUG
    klog_print("scheduler\n");
#endif
    if (!emptyProcQ(&ready_queue)) {
#ifdef DEBUG
        klog_print("scheduler: Inserting new proc.\n");
        __STEP();
#endif
        running_pcb = removeProcQ(&ready_queue);
        setTIMER(TIMESLICE);
        LDST(&running_pcb->p_s);
    } else if (process_count == 0) {
#ifdef DEBUG
        klog_print("Halting.\n");
        __STEP();
#endif
        // When all processes are terminated we shut down
        HALT();
    } else if (process_count > 0 && soft_block_count == 0) {
#ifdef DEBUG
        klog_print("scheduler: Deadlock\n");
        __STEP();
#endif
        // This check is logically sound because time is
        // quantized using clock interrupts; if no process is
        // waiting for the clock or I/O then there's a deadlock.
        PANIC();
    } else {
#ifdef DEBUG
        klog_print("Scheduler: Be waitin\n");
        __STEP();
#endif
        // 3.2 Important remark
        setMIE(MIE_ALL & ~MIE_MTIE_MASK);
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);
        WAIT();
    }
}
