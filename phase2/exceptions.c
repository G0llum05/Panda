#include "../headers/const.h"
#include "../headers/listx.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"

#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

// Macro to determine if an exception is an interrupt or not.
// Works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

// Function to copy a cpu state into a process->p_s
static void copyState(state_t* cpu_state, pcb_PTR process) {
    process->p_s =
        (state_t){cpu_state->entry_hi, cpu_state->cause, cpu_state->status,
                  cpu_state->pc_epc, cpu_state->mie};
    // NOTE: an array must be copied separately
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        process->p_s.gpr[i] = cpu_state->gpr[i];
    }
}

// pass up or die sub-handler (spec 8)
static void _puodHandler(int idx) {
    support_t* support = running_pcb->p_supportStruct;
    if (!support) {
        // TODO: DIE!
    }
    // else pass up
    state_t* state = (state_t*)GET_EXCEPTION_STATE_PTR(0)->reg_a1;
    support->sup_exceptState[idx] =
        (state_t){state->entry_hi, state->cause, state->status, state->pc_epc,
                  state->mie};
    for (int i = 0; i < STATE_GPR_LEN; i++) {
        support->sup_exceptState[idx].gpr[i] = state->gpr[i];
    }

    context_t* context = support->sup_exceptContext;
    LDCXT(context->stackPtr, context->status, context->pc);
}

static void _createProcess() {
    pcb_PTR new_process = allocPcb();
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);

    if (new_process == NULL) {
        exc_state->reg_a0 = -1;
        return;
    }

    state_t* new_state = (state_t*)exc_state->reg_a1;
    support_t* new_support_struct = (support_t*)exc_state->reg_a3;

    copyState(new_state, new_process);

    // If no parameter is provided in a3, allocPCB() initializes to NULL
    if (new_support_struct != 0)
        new_process->p_supportStruct = new_support_struct;

    insertProcQ(&running_pcb->p_list, new_process);
    insertChild(running_pcb, new_process);

    // p_pid is initialized to static next_pid in allocPCB()
    // p_time, p_semAdd are initialized to zero in allocPCB()
    new_process->p_prio = exc_state->reg_a2;
    process_count++;

    exc_state->reg_a0 = new_process->p_pid;
}

// Returns a pointer to the root of all processes
static pcb_PTR _getRoot() {
    pcb_PTR temp = running_pcb;
    while (temp != NULL) {
        temp = temp->p_parent;
    }
    return temp;
}

// Returns a pcb with the given pid
static pcb_PTR _treeSearch(int pid, pcb_PTR node) {
    if (node->p_pid == pid)
        return node;

    pcb_PTR child;
    list_for_each_entry(child, &node->p_child, p_sib) {
        pcb_PTR found = _treeSearch(pid, child);
        if (found != NULL)
            return found;
    }

    return NULL;
}

// Function that terminates a process and all its children recursively
static void _termChildren(pcb_PTR node) {
    if (node == NULL)
        return;

    pcb_PTR child;
    list_for_each_entry(child, &node->p_child, p_sib) {
        _termChildren(child);
        freePcb(child);     // Returns void, so no branching
        outBlocked(child);  // Returns NULL only when child does not exist
    }
}

static void _termProcess() {
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);
    int pid = exc_state->reg_a1;

    pcb_PTR root = _getRoot();
    pcb_PTR target_pcb = _treeSearch(pid, root);

    _termChildren(target_pcb);
}

/*  NOTE:
    Positive semaphore values mean available resources.
    Negative semaphore values mean waiting processes.
    Semaphore value set to zero means no waiting/available.
*/
static void _passeren() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    int* semAdd = (int*)cpu_state->reg_a1;

    *semAdd = *semAdd - 1;

    // The running process has to wait for the resource
    if (*semAdd < 0) {
        insertBlocked(semAdd, running_pcb);
        copyState(cpu_state, running_pcb);

        // The scheduler must know that there's no running process
        // so that it can dispatch a new one properly.
        running_pcb = NULL;
        scheduler();
    }

    // If resources were available, there's no need to call a scheduler
}

static void _verhogen() {
    memaddr* semAdd = (memaddr*)GET_EXCEPTION_STATE_PTR(0)->reg_a1;

    *semAdd = *semAdd + 1;

    // If there were blocked processes, unblock one
    if (*semAdd <= 0) {
        pcb_t* removed_pcb = removeBlocked((int*)semAdd);
        if (removed_pcb != NULL)
            insertProcQ(&ready_queue, removed_pcb);
    }
}

static void _doIO() {};

static void _getCPUTime() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = running_pcb->p_time;
}

static void _clockWait() {};

static void _getSupportPtr() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = (memaddr)running_pcb->p_supportStruct;
}

static void _getProcessId() {
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);

    if (exc_state->reg_a1 == 0) {
        exc_state->reg_a0 = running_pcb->p_pid;
    } else if (running_pcb->p_parent == NULL) {
        // Spec 6.9: Calling process is root, return 0
        exc_state->reg_a0 = 0;
    } else {
        exc_state->reg_a0 = running_pcb->p_parent->p_pid;
    }
}

static void _yield() {
    // No other processes
    if (list_empty(&ready_queue))
        return;

    // Save current process state
    copyState(GET_EXCEPTION_STATE_PTR(0), running_pcb);

    // Add current process at the end of the ready queue
    list_del(&running_pcb->p_list);
    list_add_tail(&running_pcb->p_list, &ready_queue);

    // Context switch
    running_pcb = NULL;
    scheduler();
}

// syscalls sub-handler
static void _syscallHandler() {

    // get privileges
    int mode = GET_EXCEPTION_STATE_PTR(0)->status & MSTATUS_MPP_MASK;

    // privileged syscall requested
    const int status = GET_EXCEPTION_STATE_PTR(0)->reg_a0;
    if (status < 0) {
        if (mode != MSTATUS_MPP_MASK) { // review needed
            // send a trap
            setCAUSE(PRIVINSTR);
            _puodHandler(GENERALEXCEPT);
        } else {

            // increase PC value to avoid infinite syscall loops
            GET_EXCEPTION_STATE_PTR(0)->pc_epc += 4;

            switch (status) {
            case (CREATEPROCESS):
                _createProcess();
                break;
            case (TERMPROCESS):
                _termProcess();
                break;
            case (PASSEREN):
                _passeren();
                break;
            case (VERHOGEN):
                _verhogen();
                break;
            case (DOIO):
                _doIO();
                break;
            case (GETTIME):
                _getCPUTime();
                break;
            case (CLOCKWAIT):
                _clockWait();
                break;
            case (GETSUPPORTPTR):
                _getSupportPtr();
                break;
            case (GETPROCESSID):
                _getProcessId();
                break;
            case (YIELD):
                _yield();
                break;
            }

            // restore prev state
            LDST(GET_EXCEPTION_STATE_PTR(0));
        }
    } else if (status >= 24 && status <= 28) // TLB exception
        _puodHandler(PGFAULTEXCEPT);         // spec 8.3
    // non existent syscall requested, send a trap
    setCAUSE(PRIVINSTR);         // REVIEW: cause, privinstr is not correct
                                 // TODO: should we setCause something?
    _puodHandler(GENERALEXCEPT); // spec 8.{1,2}
}

// Temporary function definition in order to compile initial.c
void uTLB_RefillHandler() {
    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*)BIOSDATAPAGE);
}

// Handles all exceptions, exclusive of TLB-Refill events.
enum { EXC_SYSCALL = 8, EXC_BREAK = 9, TLB_FIRST = 24, TLB_LAST = 28 };

void exceptionHandler() {
    unsigned int cause = getCAUSE();

    if (CAUSE_IS_INT(cause)) {
        interruptHandler();
        return;
    }

    switch (CAUSE_CODE(cause)) {
    case EXC_SYSCALL:
    case EXC_BREAK:
        _syscallHandler();
        break;

    case TLB_FIRST ... TLB_LAST:
        _puodHandler(PGFAULTEXCEPT);
        break;

    default:
        _puodHandler(GENERALEXCEPT);
        break;
    }
}
