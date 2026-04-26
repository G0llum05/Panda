#include "../headers/types.h"
#include "uriscv/types.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

void uTLB_RefillHandler() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    unsigned int vpn = ENTRYHI_GET_VPN(cpu_state->entry_hi);
    // Spec 3#Technical Point: The refill handler is allowed
    // to use phase 2 structures and global variables.
    extern pcb_t* running_pcb;
    support_t* support_structure = running_pcb->p_supportStruct;
    // VPI := Virtual Page Index
    unsigned int vpi = (vpn - 0x80000000) >> 12;
    pteEntry_t page_table = support_structure->sup_privatePgTbl[vpi];
    setENTRYHI(page_table.pte_entryHI);
    setENTRYLO(page_table.pte_entryLO);
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
