#include "../headers/types.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/scheduler.h"

#include <uriscv/types.h>

unsigned int process_count;
unsigned int soft_block_count;
pcb_t* running_pcb;
struct list_head ready_queue;
int device_semaphores[SEMDEVLEN];
int pseudo_clock_semaphore;

void uTLB_RefillHandler();

int main() {
    INIT_LIST_HEAD(&ready_queue);

    passupvector_t* pass_up_vector = (passupvector_t*)0x0FFFF900;

    pass_up_vector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
    pass_up_vector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
    pass_up_vector->exception_handler = (memaddr)nucleusExceptionHandler;
    pass_up_vector->exception_stackPtr = (memaddr)KERNELSTACK;

    initPcbs();
    initASL();

    // Initialize all memory locations for semaphores
    for (int i = 0; i < SEMDEVLEN; i++) {
        device_semaphores[i] = 0;
    }

    pseudo_clock_semaphore = 0;

    process_count = 0;
    soft_block_count = 0;
    mkEmptyProcQ(&ready_queue);
    running_pcb = NULL;

    LDIT(PSECOND);

    // allocPCB() inizializza tutto a zero/NULL
    pcb_t* first_proc = allocPcb();
    process_count++;
    insertProcQ(&ready_queue, first_proc);
    first_proc->p_s.mie = MIE_ALL;
    first_proc->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
    first_proc->p_s.pc_epc = (memaddr)test;

    RAMTOP(first_proc->p_s.reg_sp);

    scheduler();

    return 0;
}
