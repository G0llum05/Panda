#include "../headers/const.h"
#include "../headers/listx.h"
#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"
#include "phase1/headers/asl.h"
#include "phase1/headers/pcb.h"
#include "uriscv/liburiscv.h"
#include "uriscv/types.h"

// macro to determine if an exception is an interrupt or not
// works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

// pass up or die sub-handler
static void _puodHandler(int idx) {}


static void _createProcess() {
    pcb_PTR new_process = allocPcb();
    state_t * processor_exception_state = GET_EXCEPTION_STATE_PTR(0);
    
    if (new_process == NULL) {
        processor_exception_state->reg_a0 = -1; 
        return;
    }
    
    state_t * new_process_state = (state_t *) processor_exception_state->reg_a1;
    support_t * new_support_struct = (support_t *) processor_exception_state->reg_a3;


    new_process->p_s = *new_process_state;

    // If no parameter is provided in a3, allocPCB() initializes to NULL
    if (new_support_struct != 0) new_process->p_supportStruct = new_support_struct;
    
    insertProcQ(&running_pcb->p_list, new_process);
    insertChild(running_pcb, new_process);

    // p_pid is initialized to static next_pid in allocPCB()
    // p_time, p_semAdd is initialized to zero in allocPCB()
    new_process->p_prio = processor_exception_state->reg_a2;
    process_count++;

    processor_exception_state->reg_a0 = new_process->p_pid;
};

static void _termProcess() {
    state_t * processor_exception_state = GET_EXCEPTION_STATE_PTR(0);
    int pid = processor_exception_state->reg_a1;

    // pcb_PTR temp_pcb = &pcbFree_table[0];
    // list_for_each()
    
};

static void _passeren() {
    int * semAdd = (int *) GET_EXCEPTION_STATE_PTR(0)->reg_a1; 
    if (*semAdd > 0) {
        *semAdd = *semAdd - 1; 
    } else {
        insertBlocked(semAdd, running_pcb);
        scheduler();
    }
};

static void _verhogen() {
    int * semAdd = (int *) GET_EXCEPTION_STATE_PTR(0)->reg_a1;

    *semAdd = *semAdd + 1;
    removeBlocked(semAdd);
    // Here we should invoke the scheduler only when
    // there actually are waiting processes (semAdd <= 0) so
    // that it can decide, based on priority, which one gets
    // to use the resource.
    if (*semAdd <= 0) scheduler();
};

static void _doIO() {};

static void _getCPUTime() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = running_pcb->p_time;
};

static void _clockWait() {};

static void _getSupportPtr() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = (memaddr) running_pcb->p_supportStruct;
};

static void _getProcessId() {
    state_t * processor_exception_state = GET_EXCEPTION_STATE_PTR(0);
    if(processor_exception_state->reg_a1 == 0) {
        processor_exception_state->reg_a0 = running_pcb->p_pid;
    } else {
        if(running_pcb->p_parent == NULL) {
            processor_exception_state->reg_a0 = 0;
        } else {
            processor_exception_state->reg_a0 = running_pcb->p_parent->p_pid;
        }
    }
};

// CHECK: CPU state registers should be preserved in pcb_t->p_s
static void _yield() {
    if (!list_empty(&ready_queue)) {
        pcb_PTR ready_pcb = running_pcb->p_list.next;
        list_del(&running_pcb->p_list);
        running_pcb = ready_pcb;
    }
};

// syscalls sub-handler
static void _syscallHandler() {

    // get privileges
    int mode = GET_EXCEPTION_STATE_PTR(0)->status & MSTATUS_MPP_MASK;

    // privileged syscall requested
    if(GET_EXCEPTION_STATE_PTR(0)->reg_a0 < 0) {
        if(mode != MSTATUS_MPP_MASK) { // review needed
            // send a trap
            setCAUSE(PRIVINSTR);
            _puodHandler(GENERALEXCEPT);
        } else {

            // increase PC value to avoid infinite syscall loops
            GET_EXCEPTION_STATE_PTR(0)->pc_epc += 4;

            switch(GET_EXCEPTION_STATE_PTR(0)->reg_a0) {
                case(CREATEPROCESS):
                    _createProcess();
                    break;
                case(TERMPROCESS):
                    _termProcess();
                    break;
                case(PASSEREN):
                    _passeren();
                    break;
                case(VERHOGEN):
                    _verhogen();
                    break;
                case(DOIO):
                    _doIO();
                    break;
                case(GETTIME):
                    _getCPUTime();
                    break;
                case(CLOCKWAIT):
                    _clockWait();
                    break;
                case(GETSUPPORTPTR):
                    _getSupportPtr();
                    break;
                case(GETPROCESSID):
                    _getProcessId();
                    break;
                case(YIELD):
                    _yield();
                    break;
            }

            // restore prev state
            LDST(GET_EXCEPTION_STATE_PTR(0));
        }
    }

    // non existent syscall requested, send a trap
    setCAUSE(PRIVINSTR); // review cause, privinstr is not correct
    _puodHandler(GENERALEXCEPT);

}

// temporary function definition in order to compile initial.c
void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*) BIOSDATAPAGE);
}

// Handles all exceptions, exclusive of TLB-Refill events.
void exceptionHandler() {
    unsigned int cause = getCAUSE();

    if(CAUSE_IS_INT(cause)) {
        interruptHandler();
    } else
    switch(CAUSE_CODE(cause)) {

        // syscalls
        case(8):
        case(11):
            _syscallHandler();
            break;
        
        // puod - tlb
        case(24):
        case(25):
        case(26):
        case(27):
        case(28):
            _puodHandler(PGFAULTEXCEPT);
            break;
        
        // puod - traps
        default:
            _puodHandler(GENERALEXCEPT);
            break;
        
    }
}