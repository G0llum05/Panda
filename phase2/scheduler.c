#include "headers/scheduler.h"

#include "uriscv/liburiscv.h"
#include <uriscv/types.h>

#include "../headers/const.h"
#include "../phase1/headers/pcb.h"

#include "headers/initial.h"

void scheduler() {
    if (!emptyProcQ(&ready_queue)) {
        running_pcb = removeProcQ(&ready_queue);
        setTIMER(TIMESLICE);
        LDST(&running_pcb->p_s);
    } else if (process_count == 0)
        // When all processes are terminated we shut down
        HALT();
    else if (process_count > 0 && soft_block_count == 0)
        // This check is logically sound because time is
        // quantized using clock interrupts; if no process is
        // waiting for the clock or I/O then there's a deadlock.
        PANIC();
    else
        WAIT();
}
