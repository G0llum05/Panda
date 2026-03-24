#include "../headers/const.h"
#include "../headers/listx.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/klog.h"
#include "headers/scheduler.h"
#include "headers/shared.h"

#include <uriscv/const.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

// Macro to determine if an exception is an interrupt or not.
// Works by checking the MSB of the passed register
#define CAUSE_IS_INT(cause) (((cause) & 0x80000000) != 0)

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

static void _createProcess() {
    pcb_PTR new_process = allocPcb();
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);

    if (new_process == NULL) {
        exc_state->reg_a0 = -1;
        return;
    }

    state_t* new_state = (state_t*)exc_state->reg_a1;
    support_t* new_support_struct = (support_t*)exc_state->reg_a3;

    _copyState(new_state, &new_process->p_s);

    // If no parameter is provided in a3, allocPCB() initializes to NULL
    if (new_support_struct != 0)
        new_process->p_supportStruct = new_support_struct;

    insertProcQ(&ready_queue, new_process);
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
    while (temp->p_parent != NULL) {
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

    // Kill children of node
    pcb_PTR child;
    list_for_each_entry(child, &node->p_child, p_sib) {
        _termChildren(child);
        freePcb(child);    // Returns void, so no branching
        outBlocked(child); // Returns NULL only when child does not exist
    }

    // Kill node
    freePcb(node);
    outBlocked(node);
}

static void _termProcess() {
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);
    int pid = exc_state->reg_a1;

    pcb_PTR target_pcb = running_pcb; 

    if (pid != 0) {
        pcb_PTR root = _getRoot();
        target_pcb = _treeSearch(pid, root);
    }

    _termChildren(target_pcb); // this kills target_pcb as well
    
    if (pid == 0)
        scheduler();
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

static void _reusablePasseren(state_t* cpu_state, int* semAdd) {
    *semAdd = *semAdd - 1;

    // The running process has to wait for the resource
    if (*semAdd < 0) {
        insertBlocked(semAdd, running_pcb);
        soft_block_count++;
        _copyState(cpu_state, &running_pcb->p_s);

        _updateTime(running_pcb);

        // The scheduler must know that there's no running process
        // so that it can dispatch a new one properly.
        running_pcb = NULL;
        scheduler();
    }

    // If resources were available, there's no need to call a scheduler
}

/*  NOTE:
    Positive semaphore values mean available resources.
    Negative semaphore values mean waiting processes.
    Semaphore value set to zero means no waiting/available.
*/
static void _passeren() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    int* semAdd = (int*)cpu_state->reg_a1;

    _reusablePasseren(cpu_state, semAdd);
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

static void _doIO() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);

    // Get the index (0-39) of the semaphore associated to the device
    int semaphore_index = (cpu_state->reg_a1 - START_DEVREG) / sizeof(devreg_t);

    // The device is a terminal device, and we need to know if
    // it's a command to transmit or receive
    if (semaphore_index >= 32) {
        termreg_t* terminal_address =
            (termreg_t*)(START_DEVREG + (semaphore_index * sizeof(devreg_t)));

        memaddr* commandp = &terminal_address->recv_command;
        memaddr* statusp = &terminal_address->recv_status;

        // If the command's address is the transmitter register, we
        // need to use the terminal output semaphore
        if (commandp != (unsigned int*)cpu_state->reg_a1) {
            // NOTE: view initial.h interrupt line map
            semaphore_index += 8;

            statusp = &terminal_address->transm_status;
            commandp = &terminal_address->transm_command;
        }

        // We need to tell the terminal the command stored in reg_a2
        if (*statusp == READY || *statusp == 5)
            *commandp = cpu_state->reg_a2;
        // REVIEW: If terminal is not ready we shouldn't wait.
        // Access should be regulated via a mutex, not busy waiting.
        // See print in p2test.c
        // REVIEW: In case of error we return the status word != ready
        else {
            cpu_state->reg_a0 = *statusp;
            return;
        }
    } else {
        dtpreg_t* device_address =
            (dtpreg_t*)(START_DEVREG + (semaphore_index * sizeof(devreg_t)));

        if (device_address->status == READY)
            device_address->command = cpu_state->reg_a2;
        else {
            cpu_state->reg_a0 = device_address->status;
            return;
        }
    }

    // Now we perform a passeren on the target semaphore

    _reusablePasseren(cpu_state, &device_semaphores[semaphore_index]);

    // NOTE: it is a job of the interrupt handler to return the value
    // in a0 upon completing terminal I/O.
};

static void _getCPUTime() {
    GET_EXCEPTION_STATE_PTR(0)->reg_a0 = running_pcb->p_time;
}

static void _clockWait() {
    _reusablePasseren(GET_EXCEPTION_STATE_PTR(0), &pseudo_clock_semaphore);
};

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
    _copyState(GET_EXCEPTION_STATE_PTR(0), &running_pcb->p_s);

    // Add current process at the end of the ready queue
    list_del(&running_pcb->p_list);
    // REVIEW: we don't use insertChild() here because of spec 6.10:
    // "the yielded process is not immediately re-executed even
    // if it has the highest priority"
    list_add_tail(&running_pcb->p_list, &ready_queue);

    _updateTime(running_pcb);

    // Call a ready process
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
