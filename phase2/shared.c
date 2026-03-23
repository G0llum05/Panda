#include "headers/shared.h"
#include "../headers/types.h"
#include "headers/klog.h"
#include "headers/scheduler.h"
#include "uriscv/const.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

void _copyState(state_t* src, state_t* dest) {
    // Consider a struct as an array of u_int
    unsigned int* dest_ptr = (unsigned int*)dest;
    unsigned int* src_ptr = (unsigned int*)src;
    unsigned int num_words = sizeof(state_t) / sizeof(unsigned int);

    for (int i = 0; i < num_words; i++)
        dest_ptr[i] = src_ptr[i];
}

inline void _exit() {
    extern pcb_t* running_pcb;
    if (running_pcb)
        LDST(&running_pcb->p_s);
    else
        scheduler();
}

void _updateTime(pcb_PTR p) {
    cpu_t current_time;
    STCK(current_time);
    p->p_time += current_time - start_time;
}