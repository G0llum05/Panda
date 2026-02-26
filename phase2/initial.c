#include "headers/initial.h"
#include "../headers/types.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "headers/exceptions.h"

unsigned int process_count;
unsigned int soft_block_count;
pcb_PTR running_pcb;
struct list_head ready_queue;

int main() {
    INIT_LIST_HEAD(&ready_queue);

    ppv_t * pass_up_vector = (ppv_t *) 0x0FFFF900;

    pass_up_vector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    pass_up_vector->tlb_stack_pointer = (memaddr)KERNELSTACK;
    pass_up_vector->exception_handler = (memaddr)exceptionHandler;
    pass_up_vector->exception_stack_pointer = (memaddr)KERNELSTACK;

    initPcbs();
    initASL();

    process_count = 0;
    soft_block_count = 0;
    mkEmptyProcQ(&ready_queue);
    running_pcb = NULL;

}