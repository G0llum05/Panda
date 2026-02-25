#include "headers/initial.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"

int main() {
    unsigned int process_count;
    unsigned int soft_block_count;
    LIST_HEAD(ready_queue);
    pcb_PTR running_pcb;

    ppv_t * pass_up_vector = (ppv_t *) 0x0FFFF900;

    pass_up_vector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
    pass_up_vector->tlb_stack_pointer = (memaddr)KERNELSTACK;
    pass_up_vector->exception_handler = (memaddr)exceptionHandler;
    pass_up_vector->exception_stack_pointer = (memaddr)KERNELSTACK;
}