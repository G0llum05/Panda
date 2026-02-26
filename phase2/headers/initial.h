#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/types.h"

// Global variables
extern unsigned int process_count;
extern unsigned int soft_block_count;
extern pcb_PTR running_pcb;
extern struct list_head ready_queue;

typedef struct {
    memaddr tlb_refill_handler;
    memaddr tlb_stack_pointer;
    memaddr exception_handler;
    memaddr exception_stack_pointer;
} ppv_t;

#endif
