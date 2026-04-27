#include "headers/vmSupport.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "headers/sysSupport.h"
#include "uriscv/arch.h"
#include "uriscv/const.h"
#include "uriscv/types.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

// Spec 5.4
static unsigned int frame_to_pick = 0;

// Spec 12.2: "[...] the Swap Pool table is local to this module."
static swap_t swap_pool_table[SWAPPOOLSIZE];
static unsigned int swap_pool_mutex = 1;

static void _handleStatus(unsigned int*);

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
    SYSCALL(PASSEREN, (unsigned int)&swap_pool_mutex, 0, 0);

    // Step 5 - Missing page to load
    unsigned int missing_page =
        ENTRYHI_GET_VPN(support_structure->sup_exceptState[0].entry_hi);

    // Step 6-7-8

    // Spec 5.3: to achieve atomic operations we disable and
    // enable interrupts.
    // Spec 5.4: the pager algorithm is round robin, FIFO
    // policy. It uses a static variable modulo the size of
    // the swap pool to choose the next frame sequentially.

    swap_t* swapp_entry = &swap_pool_table[frame_to_pick];
    pteEntry_t* process_pte = swapp_entry->sw_pte;
    dtpreg_t* dev_addr =
        (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, support_structure->sup_asid);
    unsigned int* status_code;
    memaddr swap_frame = ENTRYLO_GET_PFN(swapp_entry->sw_pte->pte_entryLO);
    if (swapp_entry->sw_asid != -1) { // A user process uses this frame
        setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
        SETBITOFF(process_pte->pte_entryLO, ENTRYLO_VALID_BIT);
        // NOTE: TLBP() uses entryHi to match the entry in TLB
        setENTRYHI(process_pte->pte_entryHI);
        TLBP();
        // Is the frame cached in the TLB?
        if ((getINDEX() & PROBEBIT) == 0) { // If 0, frame found
            setENTRYLO(process_pte->pte_entryLO);
            TLBWI();
        }
        setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

        // Save old frame
        dev_addr->data0 = swap_frame;
        status_code =
            (unsigned int*)SYSCALL(DOIO, (int)dev_addr, FLASHWRITE, 0);

        _handleStatus(status_code);
    }

    // Step 9
    // Load missing page into memory
    dev_addr->data0 = missing_page; // REVIEW: input? Domain: flash address
    dev_addr->data1 = swap_frame;   // REVIEW: output? Co-Dom: physical mem
    status_code = (unsigned int*)SYSCALL(DOIO, (int)dev_addr, FLASHREAD, 0);
    _handleStatus(status_code);

    // Step 10
    swapp_entry->sw_asid = support_structure->sup_asid;
    process_pte = &support_structure->sup_privatePgTbl[missing_page];

    // Step 11
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);

    process_pte->pte_entryLO &= ~ENTRYLO_PFN_MASK;
    SETBITON(process_pte->pte_entryLO, ENTRYLO_VALID_BIT);
    process_pte->pte_entryLO |= (swap_frame & ENTRYLO_PFN_MASK);

    // Step 12

    setENTRYHI(process_pte->pte_entryHI);
    TLBP();
    // Is the frame cached in the TLB?
    if ((getINDEX() & PROBEBIT) == 0) { // If 0, frame found
        setENTRYLO(process_pte->pte_entryLO);
        TLBWI();
    }

    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

    frame_to_pick = (frame_to_pick + 1) % SWAPPOOLSIZE;

    // Step 13

    SYSCALL(VERHOGEN, (unsigned int)&swap_pool_mutex, 0, 0);

    // Step 14
    LDST(&support_structure->sup_exceptState[0]);
}

static void _handleStatus(unsigned int* status_code) {
    switch (*status_code) {
    case UNINSTALLED:
    case READY:
    case BUSY:
        // TODO: see what to do in these cases
        break;
    default:
        supportExceptionHandler();
    }
}