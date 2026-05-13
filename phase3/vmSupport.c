#include "headers/vmSupport.h"
#include "../headers/const.h"
#include "../headers/listx.h"
#include "../headers/types.h"
#include "headers/initProc.h"
#include "headers/sysSupport.h"
#include "uriscv/arch.h"
#include "uriscv/const.h"
#include "uriscv/types.h"
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

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

// Is the pool empty?
int isSupportPoolEmpty() { return list_empty(&supportsFree); }

// Spec 12.2: "The test function will now invoke [...] initSwapStructs
// which will do the work of initializing the Swap Pool table."
void initSwapStructs() {
    for (int i = 0; i < SWAPPOOLSIZE; i++) {
        swap_pool_table[i].sw_asid = -1;
    }
}

// Spec 3
unsigned int next_tlbi = 0;
void uTLB_RefillHandler() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    unsigned int vpn = ENTRYHI_GET_VPN(cpu_state->entry_hi);
    // Spec 3#Technical Point: The refill handler is allowed
    // to use phase 2 structures and global variables.
    extern pcb_t* running_pcb;
    support_t* support_structure = running_pcb->p_supportStruct;
    pteEntry_t* page_table_entry =
        &support_structure->sup_privatePgTbl[vpn > 30 ? 31 : vpn];
    setENTRYHI(page_table_entry->pte_entryHI);
    setENTRYLO(page_table_entry->pte_entryLO);
    TLBP();
    if ((getINDEX() & PROBEBIT) != 0) {
        // entry not found, round robinize
        setINDEX(next_tlbi << 8);
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
    unsigned int vpi = vpn > 30 ? 31 : vpn;

    // Step 6-7-8

    // Spec 5.3: to achieve atomic operations we disable and
    // enable interrupts.
    // Spec 5.4: the pager algorithm is round robin, FIFO
    // policy. It uses a static variable modulo the size of
    // the swap pool to choose the next frame sequentially.

    unsigned int frame_to_pick = 0;
    for (; frame_to_pick < SWAPPOOLSIZE; frame_to_pick++) {
        if (swap_pool_table[frame_to_pick].sw_pte->pte_entryLO &
            ENTRYLO_VALID_BIT) {
            break;
        }
    }
    swap_t* swap_pte = &swap_pool_table[frame_to_pick];
    int process_asid = swap_pte->sw_asid;

    // TODO: re-rename variables
    pteEntry_t* process_pte = swap_pte->sw_pte;
    int flash_idx = support_structure->sup_asid - 1; // NOTE: flashNo + 1 = ASID
    dtpreg_t* dev_addr = (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, flash_idx);
    int old_page_flash_idx = process_asid - 1;
    dtpreg_t* old_dev_addr =
        (dtpreg_t*)DEV_REG_ADDR(IL_FLASH, old_page_flash_idx);
    memaddr old_commandp = (memaddr)&old_dev_addr->command;
    unsigned int status_code;
    memaddr swap_frame_addr = FLASHPOOLSTART + frame_to_pick * PAGESIZE;
    unsigned int block_idx = swap_pte->sw_pageNo;

    if (process_asid != -1) { // A user process uses this frame
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
        old_dev_addr->data0 = swap_frame_addr;

        SYSCALL(PASSEREN, (int)&support_mutex[old_page_flash_idx], 0, 0);

        // REVIEW: is page-block mapping 1-1? (apparently yes)
        // Block could be 1024, page is 4096
        status_code =
            SYSCALL(DOIO, (int)old_commandp, (block_idx << 8) | FLASHWRITE, 0);

        SYSCALL(VERHOGEN, (int)&support_mutex[old_page_flash_idx], 0, 0);

        _handleStatus(status_code);
    }

    // Step 9
    // Load missing page into memory
    dev_addr->data0 = swap_frame_addr;
    SYSCALL(PASSEREN, (int)&support_mutex[flash_idx], 0, 0);
    status_code =
        SYSCALL(DOIO, (memaddr)&dev_addr->command, (vpi << 8) | FLASHREAD, 0);
    SYSCALL(VERHOGEN, (int)&support_mutex[flash_idx], 0, 0);

    _handleStatus(status_code);

    // Step 10
    process_asid = support_structure->sup_asid;
    swap_pte->sw_asid = process_asid;
    swap_pte->sw_pageNo = vpi;
    process_pte = &support_structure->sup_privatePgTbl[vpi];
    swap_pte->sw_pte = process_pte;
    // Step 11
    setSTATUS(getSTATUS() & ~MSTATUS_MIE_MASK);

    process_pte->pte_entryLO &= ~ENTRYLO_PFN_MASK;
    SETBITON(process_pte->pte_entryLO, ENTRYLO_VALID_BIT);
    process_pte->pte_entryLO |= (swap_frame_addr & ENTRYLO_PFN_MASK);

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
    LDST(&support_structure->sup_exceptState[PGFAULTEXCEPT]);
}

static void _handleStatus(unsigned int status_code) {
    if (status_code != READY) {
        SYSCALL(VERHOGEN, (unsigned int)&swap_pool_mutex, 0, 0);
        programTrapHandler();
    }
}
