#include "headers/vmSupport.h"
#include "../headers/types.h"
#include "uriscv/types.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>

// Spec 5.4
static unsigned int frame_to_pick = 0;

// Spec 12.2: "[...] the Swap Pool table is local to this module."
static swap_t swap_pool_table[SWAPPOOLSIZE];
static unsigned int swap_pool_mutex = 1;

// Spec 12.2: "The test function will now invoke [...] initSwapStructs
// which will do the work of initializing the Swap Pool table."
void initSwapStructs() {}

// Spec 3
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
*/

void pager() {
    // Step 1
    support_t* support_structure = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Step 2
    unsigned int cause = support_structure->sup_exceptState[0].cause;

    // Step 3
    // REVIEW:
    // If cause == TLB_MOD the supportExceptionHandler defaults to a program
    // trap. See sysSupport.c for more details.

    // Step 4
    SYSCALL(PASSEREN, (int)&swap_pool_mutex, 0, 0);

    // Step 5
    unsigned int missing_page =
        ENTRYHI_GET_VPN(support_structure->sup_exceptState[0].entry_hi);

    // Step 6-7-8

    // Spec 5.3: to achieve atomic operations we disable and
    // enable interrupts.
    // Spec 5.4: the pager algorithm is round robin, FIFO
    // policy. It uses a static variable modulo the size of
    // the swap pool to choose the next frame sequentially.

    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);

    swap_t* swap_frame_ptr = &swap_pool_table[frame_to_pick % SWAPPOOLSIZE];

    if (swap_frame_ptr->sw_asid != -1) { // A user process uses this frame
        setENTRYHI(swap_frame_ptr->sw_pte->pte_entryHI);
        SETBITOFF(swap_frame_ptr->sw_pte->pte_entryLO, ENTRYLO_VALID_BIT);
        TLBP();
        // Is the frame cached in the TLB?
        if ((getINDEX() & PROBEBIT) == 0) {
            setENTRYLO(swap_frame_ptr->sw_pte->pte_entryLO);
            TLBWI();
        }
    }

    frame_to_pick++;
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}