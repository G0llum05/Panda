#include "headers/scheduler.h"

#include "headers/klog.h"
#include "uriscv/const.h"
#include "uriscv/liburiscv.h"
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../phase1/headers/pcb.h"

#include "headers/initial.h"

// start time since running_pcb re-started computing
cpu_t start_time;

void scheduler() {
    if (!emptyProcQ(&ready_queue)) {
        running_pcb = removeProcQ(&ready_queue);
        STCK(start_time);
        setTIMER(TIMESLICE * (*(cpu_t*)TIMESCALEADDR));
        LDST(&running_pcb->p_s);

    } else if (process_count == 0) {
        // When all processes are terminated we shut down
        HALT();

    } else if (process_count > 0 && soft_block_count > 0) {
        running_pcb = NULL;

        // 3.2 Important remark
        setMIE(MIE_ALL & ~MIE_MTIE_MASK);
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);
        WAIT();
    } else {
        // This check is logically sound because time is
        // quantized using clock interrupts; if no process is
        // waiting for the clock or I/O then there's a deadlock.
        PANIC();
    }
}
