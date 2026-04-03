#include "headers/shared.h"
#include "../headers/types.h"
#include "headers/scheduler.h"
#include "uriscv/const.h"
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

void* memcpy(void* dest, const void* src, unsigned int n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void _copyState(state_t* src, state_t* dest) {
    if (src != NULL && dest != NULL) {
        *dest = *src;
    }
}

void _updateTime(pcb_PTR p) {
    cpu_t current_time;
    STCK(current_time);
    p->p_time += current_time - start_time;
}
