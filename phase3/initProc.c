#include "headers/initProc.h"
#include "../headers/const.h"
#include "../headers/types.h"
#include "../phase2/headers/klog.h"
#include "../testers/h/string.h"
#include "headers/sysSupport.h"
#include "headers/vmSupport.h"
#include "uriscv/cpu.h"
#include "uriscv/liburiscv.h"

void* swap_pool = (void*)FLASHPOOLSTART;
unsigned int shell_mutex = 0;
unsigned int master_semaphore = 0;

// Device mutex
// Flash devices index 0-7
// Terminal input device index 8
// Terminal output device index 9
unsigned int device_mutex[FLASHDEVICES + TERMINALDEVICES];

// Instatiator process
state_t shell_state;
support_t shell_support;
void test() {
    // Initialize Swap Pool Table
    initSwapStructs();
    // Initialize shell state struct
    // STST(&shell_state);
    const unsigned int ASID = 3 << (ENTRYHI_ASID_BIT);
    shell_state.reg_sp = USERSTACKTOP;
    shell_state.mie = MIE_ALL;
    shell_state.pc_epc = (memaddr)UPROCSTARTADDR;
    shell_state.status |= MSTATUS_MPIE_MASK;
    shell_state.entry_hi = ASID;
    // Initialize shell support struct
    shell_support.sup_exceptContext[GENERALEXCEPT].stackPtr =
        (memaddr)&shell_support.sup_stackGen[499];
    shell_support.sup_exceptContext[GENERALEXCEPT].status |= MSTATUS_MPP_M;
    shell_support.sup_exceptContext[GENERALEXCEPT].pc =
        (memaddr)supportExceptionHandler;
    shell_support.sup_exceptContext[PGFAULTEXCEPT].stackPtr =
        (memaddr)&shell_support.sup_stackTLB[499];
    shell_support.sup_exceptContext[PGFAULTEXCEPT].status |= MSTATUS_MPP_M;
    shell_support.sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)pager;
    shell_support.sup_asid = 3;

    // NOTE: Don't move this define
#define SET_ENTRYHI(i, x) shell_support.sup_privatePgTbl[(i)].pte_entryHI |= (x)
#define SET_ENTRYLO(i, x) shell_support.sup_privatePgTbl[(i)].pte_entryLO |= (x)

    for (unsigned int i = 0; i < USERPGTBLSIZE - 1; i++) {
        // NOTE: flash x -> asid x+1
        SET_ENTRYHI(i, KUSEG + (i << ENTRYHI_VPN_BIT));
        SET_ENTRYHI(i, ASID);
        SET_ENTRYLO(i, DIRTYON);
    }
    // NOTE: cf. 2.1
    SET_ENTRYHI(31, ASID);
    SET_ENTRYHI(31, 0xBFFFF << ENTRYHI_VPN_BIT);
    SET_ENTRYLO(31, DIRTYON);

    //  SET entryLO dirty

    // Create the shell
    SYSCALL(CREATEPROCESS, (int)&shell_state, PROCESS_PRIO_LOW,
            (int)&shell_support);

    klog_print("P");
    // master_semaphore.P() to start shell
    SYSCALL(PASSEREN, (int)&master_semaphore, 0, 0);

    // System halt
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
