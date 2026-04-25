#include "../headers/types.h"
#include <uriscv/liburiscv.h>

void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*)BIOSDATAPAGE);
}

/*
    Spec 5.3:

    Updating the page table of a process requires
    also updating the associated tlb entry in the tlb.
    Either:
    1. Delete all the TLB
    2. Probe the TLB and rewrite the entry if present

    IMPORTANT: to update the pager atomically we must
    first disable interrupts and the enable them back.
    disable: setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK)
    enable: setSTATUS(getSTATUS() | MSTATUS_MIE_MASK))
*/

// Spec 5.4:
// The pager algorithm is FIFO and uses a static variable
// modulo the size of the swap pool (round robin)
