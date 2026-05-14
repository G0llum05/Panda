#include "headers/vmSupport.h"
#include "../headers/const.h"
#include "../headers/listx.h"
#include "../headers/types.h"
#include "headers/initProc.h"
#include "headers/sysSupport.h"
#include "uriscv/arch.h"
#include "uriscv/const.h"
#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"
#include "uriscv/types.h"

// Spec 10: create allocate, deallocate methods to manipulate
// static structures.

/*
    Flash device / ASID associations are in the config file for uriscv.
    "flash0": "testers/shell.uriscv",
    "flash1": "testers/fibEight.uriscv",
    "flash2": "testers/echo.uriscv",
    "flash3": "testers/fibEleven.uriscv",
    "flash4": "testers/uname.uriscv",
    "flash5": "testers/date.uriscv",
    "flash6": "testers/sl.uriscv",
    "flash7": "testers/calc.uriscv"
*/

static support_t supports[UPROCMAX];
static struct list_head supportsFree;

// Spec 12.2: "[...] the Swap Pool table is local to this module."
static swap_t swap_pool_table[SWAPPOOLSIZE];
static unsigned int swap_pool_mutex = 1;
static int initializedDirtyness[UPROCMAX];

static void _handleStatus(unsigned int);

void setState(state_t* state, unsigned int asid) {
    state->reg_sp = USERSTACKTOP;
    state->mie = MIE_ALL;
    state->pc_epc = (memaddr)UPROCSTARTADDR;
    state->status = MSTATUS_MPIE_MASK;
    state->entry_hi = asid << ENTRYHI_ASID_BIT;
}

// Initialize the support struct free list (call once at startup)
void initSupportPool() {
    INIT_LIST_HEAD(&supportsFree);
    for (int i = 0; i < 8; i++) {
        INIT_LIST_HEAD(&supports[i].s_list);
        list_add_tail(&supports[i].s_list, &supportsFree);
    }
}

// Get a free support struct from the pool, or NULL if none available
support_t* allocSupportStruct() {
    if (list_empty(&supportsFree))
        return NULL;
    struct list_head* node = supportsFree.next;
    list_del(node);
    support_t* sup = container_of(node, support_t, s_list);
    INIT_LIST_HEAD(&sup->s_list);
    return sup;
}

// Return a used support struct to the free pool
void freeSupportStruct(support_t* sup) {
    list_del(&sup->s_list);
    INIT_LIST_HEAD(&sup->s_list);
    list_add_tail(&sup->s_list, &supportsFree);
}

void invalidateSwapPoolByASID(unsigned int asid) {
    for (int i = 0; i < SWAPPOOLSIZE; i++) {
        if (swap_pool_table[i].sw_asid == (int)asid) {
            swap_pool_table[i].sw_asid = -1;
            swap_pool_table[i].sw_pte = NULL;
        }
    }
}

void invalidateTLBBySupport(support_t* sup) {
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
    for (int i = 0; i < USERPGTBLSIZE; i++) {
        pteEntry_t* entry = &sup->sup_privatePgTbl[i];
        setENTRYHI(entry->pte_entryHI);
        TLBP();
        if ((getINDEX() & PROBEBIT) == 0) { // entry trovata in TLB
            SETBITOFF(entry->pte_entryLO, ENTRYLO_VALID_BIT);
            setENTRYLO(entry->pte_entryLO);
            TLBWI();
        }
    }
    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);
}

int isSupportPoolEmpty() { return list_empty(&supportsFree); }

// Spec 12.2: "The test function will now invoke [...] initSwapStructs
// which will do the work of initializing the Swap Pool table."
void initSwapStructs() {
    for (int i = 0; i < SWAPPOOLSIZE; i++) {
        swap_pool_table[i].sw_asid = -1;
    }
}

// Spec 3
static unsigned int next_tlbi = 0;
void uTLB_RefillHandler() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    unsigned int vpn = ENTRYHI_GET_VPN(cpu_state->entry_hi);
    // Spec 3#Technical Point: The refill handler is allowed
    // to use phase 2 structures and global variables.
    extern pcb_t* running_pcb;
    support_t* support_structure = running_pcb->p_supportStruct;
    pteEntry_t* page_table_entry =
        &support_structure
             ->sup_privatePgTbl[vpn > LASTPAGEINDEX ? STACKINDEX : vpn];
    setENTRYHI(page_table_entry->pte_entryHI);
    setENTRYLO(page_table_entry->pte_entryLO);
    TLBP();
    if ((getINDEX() & PROBEBIT) != 0) {
        // Entry not found, choose a new entry (round robin policy)
        setINDEX(next_tlbi << TLBINDEXBIT);
        next_tlbi = (next_tlbi + 1) % POOLSIZE;
    }
    TLBWI();
    LDST((state_t*)cpu_state);
}

void pager() {
    // Step 1
    support_t* support_structure = (support_t*)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    // Step 2-3
    unsigned int cause =
        support_structure->sup_exceptState[PGFAULTEXCEPT].cause;

    // If cause == TLB_MOD the supportExceptionHandler defaults to a program
    // trap. See sysSupport.c for more details.
    if (cause == EXC_MOD) {
        programTrapHandler();
    }

    // Step 4
    SYSCALL(PASSEREN, (unsigned int)&swap_pool_mutex, 0, 0);

    // Step 5 - Missing page to load
    unsigned int vpn = ENTRYHI_GET_VPN(
        support_structure->sup_exceptState[PGFAULTEXCEPT].entry_hi);
    unsigned int pageNo = vpn > LASTPAGEINDEX ? STACKINDEX : vpn;

    // Step 6-7-8
    // Spec 5.3: to achieve atomic operations we disable and
    // enable interrupts.

    // Finds first free swap page in O(n)
    unsigned int frame_to_pick = 0;
    for (; frame_to_pick < SWAPPOOLSIZE; frame_to_pick++) {
        if (swap_pool_table[frame_to_pick].sw_pte->pte_entryLO &
            ENTRYLO_VALID_BIT) {
            break;
        }
    }

    memaddr swap_frame_addr = FLASHPOOLSTART + frame_to_pick * PAGESIZE;
    swap_t* swap_pte = &swap_pool_table[frame_to_pick];

    int old_process_asid = swap_pte->sw_asid; // Occupying process asid
    if (old_process_asid != -1) {             // A user process uses this frame
        int flash = old_process_asid - 1;
        pteEntry_t* pte = swap_pte->sw_pte;
        unsigned int pageNo = swap_pte->sw_pageNo;
        dtpreg_t* dev_addr = (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, flash);
        memaddr commandp = (memaddr)&dev_addr->command;

        setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);
        SETBITOFF(pte->pte_entryLO, ENTRYLO_VALID_BIT);
        // NOTE: TLBP() uses entryHi to match the entry in TLB
        setENTRYHI(pte->pte_entryHI);
        TLBP();
        // Is the frame cached in the TLB?
        if ((getINDEX() & PROBEBIT) == 0) { // If 0, frame found
            setENTRYLO(pte->pte_entryLO);
            TLBWI();
        }
        setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

        // Save old frame
        dev_addr->data0 = swap_frame_addr;

        SYSCALL(PASSEREN, (int)&support_mutex[flash], 0, 0);

        int status_code =
            SYSCALL(DOIO, (int)commandp, (pageNo << 8) | FLASHWRITE, 0);

        SYSCALL(VERHOGEN, (int)&support_mutex[flash], 0, 0);

        _handleStatus(status_code);
    }

    unsigned int status_code;
    unsigned int asid = support_structure->sup_asid;
    pteEntry_t* pte = &support_structure->sup_privatePgTbl[pageNo];

    // Step 9
    // Load missing page into memory
    int flash = asid - 1; // NOTE: flashNo + 1 = ASID
    dtpreg_t* dev_addr = (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, flash);

    dev_addr->data0 = swap_frame_addr;
    SYSCALL(PASSEREN, (int)&support_mutex[flash], 0, 0);
    status_code = SYSCALL(DOIO, (memaddr)&dev_addr->command,
                          (pageNo << 8) | FLASHREAD, 0);
    SYSCALL(VERHOGEN, (int)&support_mutex[flash], 0, 0);

    _handleStatus(status_code);

    // Creative Step: If page contains header, set text to read-only
    if (pageNo == 0 && !initializedDirtyness[asid - 1]) {
        initializedDirtyness[asid - 1] = 1;
        typedef unsigned int uint;
        uint text_start_block =
            (*(uint*)(swap_frame_addr + TEXT_START_POINTER_OFFSET)) / PAGESIZE;
        uint text_of = (*(uint*)(swap_frame_addr + TEXT_FILE_SIZE)) / PAGESIZE;
        for (int i = 0; i < STACKINDEX; i++) {

            if (text_start_block <= i && i < text_start_block + text_of) {
                // Set read-only
                pteEntry_t* pte = &support_structure->sup_privatePgTbl[i];
                SETBITOFF(pte->pte_entryLO, ENTRYLO_DIRTY_BIT);
            } else {
                pteEntry_t* pte = &support_structure->sup_privatePgTbl[i];
                SETBITON(pte->pte_entryLO, ENTRYLO_DIRTY_BIT);
            }
        }
    }

    // Step 10
    // Initialize swap_pte
    swap_pte->sw_asid = asid;
    swap_pte->sw_pageNo = pageNo;
    swap_pte->sw_pte = pte;

    // Step 11
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);

    pte->pte_entryLO &= ~ENTRYLO_PFN_MASK;
    SETBITON(pte->pte_entryLO, ENTRYLO_VALID_BIT);
    pte->pte_entryLO |= (swap_frame_addr & ENTRYLO_PFN_MASK);

    // Step 12
    setENTRYHI(pte->pte_entryHI);
    TLBP();
    // Is the frame cached in the TLB?
    if ((getINDEX() & PROBEBIT) == 0) { // If 0, frame found
        setENTRYLO(pte->pte_entryLO);
        TLBWI();
    }

    setSTATUS(getSTATUS() | MSTATUS_MIE_MASK);

    // Step 13
    SYSCALL(VERHOGEN, (unsigned int)&swap_pool_mutex, 0, 0);

    // Step 14
    LDST(&support_structure->sup_exceptState[PGFAULTEXCEPT]);
}

static inline void _handleStatus(unsigned int status_code) {
    if (status_code != READY) {
        SYSCALL(VERHOGEN, (unsigned int)&swap_pool_mutex, 0, 0);
        programTrapHandler();
    }
}
