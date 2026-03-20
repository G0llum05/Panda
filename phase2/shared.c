#include "headers/shared.h"
#include "headers/klog.h"

void _copyState(state_t* src, state_t* dest) {
#ifdef DEBUG
    klog_print("copyState\n");
#endif
    // Consider a struct as an array of u_int
    unsigned int* dest_ptr = (unsigned int*)dest;
    unsigned int* src_ptr = (unsigned int*)src;
    unsigned int num_words = sizeof(state_t) / sizeof(unsigned int);

    for (int i = 0; i < num_words; i++)
        dest_ptr[i] = src_ptr[i];
}
