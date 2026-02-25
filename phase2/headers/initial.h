#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/types.h"

typedef struct {
    memaddr tlb_refill_handler;
    memaddr tlb_stack_pointer;
    memaddr exception_handler;
    memaddr exception_stack_pointer;
} ppv_t;

#endif
