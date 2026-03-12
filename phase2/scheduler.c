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
    } else {
        if (process_count == 0) {
            HALT();
        } else if (process_count > 0 && soft_block_count == 0) {
            WAIT();
        } else {
            PANIC();
        }
    }
}
