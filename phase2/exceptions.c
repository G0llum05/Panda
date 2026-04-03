#include "../headers/const.h"
#include "../headers/listx.h"

#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"

#include "headers/exceptions.h"
#include "headers/initial.h"
#include "headers/interrupts.h"
#include "headers/scheduler.h"
#include "headers/shared.h"

#include <uriscv/const.h>
#include <uriscv/cpu.h>
#include <uriscv/liburiscv.h>
#include <uriscv/types.h>

#define CAUSE_CODE(cause) ((cause) & GETEXECCODE)

static void _syscallHandler();
static void _createProcess();
static void _termProcess();
static void _puodHandler(int idx);
static void _term(pcb_t* proc);
static void _passeren();
static void _verhogen();
static void _doIO();
static void _getCPUTime();
static void _clockWait();
static void _getSupportPtr();
static void _getProcessId();
static void _yield();

// Handles all exceptions, exclusive of TLB-Refill events
void exceptionHandler() {
    unsigned int cause = getCAUSE();

    if (CAUSE_IS_INT(cause)) {
        if (running_pcb) { // if there's a running process, we save its state
                           // before handling the interrupt
            _copyState(GET_EXCEPTION_STATE_PTR(0), &running_pcb->p_s);
        }
        interruptHandler();
        return;
    }

    switch (cause) {
    case EXC_ECU:
    case EXC_ECM:
        _syscallHandler();
        break;

    case EXC_MOD ... EXC_UTLBS:
        _puodHandler(PGFAULTEXCEPT);
        break;

    default:
        _puodHandler(GENERALEXCEPT);
        break;
    }
}

static void _syscallHandler() {
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);
    // Store the Machine Previous Privilege mode
    int mode = exc_state->status & MSTATUS_MPP_MASK;

    const int syscall_code = exc_state->reg_a0;

    // Increase PC value and go to next instruction
    exc_state->pc_epc += 4;

    // If it is a privileged syscall, was the previous state the Machine mode?
    if (syscall_code < 0 && (mode == MSTATUS_MPP_M)) {
        switch (syscall_code) {
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

        // Restore previous state
        LDST(exc_state);
    }

    if (mode != MSTATUS_MPP_M) {
        exc_state->cause = PRIVINSTR;
    }

    // It is not a nucleus syscall, so we pass up its handling
    _puodHandler(GENERALEXCEPT);
}

// pass up or die sub-handler (spec 8)
static void _puodHandler(int idx) {
    support_t* support = running_pcb->p_supportStruct;
    if (!support) {
        _term(running_pcb);
        scheduler();
    }
    // else pass up
    state_t* state = GET_EXCEPTION_STATE_PTR(0);

    _copyState(state, &support->sup_exceptState[idx]);

    context_t* context = &support->sup_exceptContext[idx];
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

    _copyState(new_state, &new_process->p_s);

    // If no parameter is provided in a3, allocPCB() initializes to NULL
    if (new_support_struct != 0)
        new_process->p_supportStruct = new_support_struct;

    new_process->p_prio = exc_state->reg_a2;

    insertProcQ(&ready_queue, new_process);
    insertChild(running_pcb, new_process);

    // p_pid is initialized to static next_pid in allocPCB()
    // p_time, p_semAdd are initialized to zero in allocPCB()
    process_count++;
    exc_state->reg_a0 = new_process->p_pid;
}

// kill proc
static void _term(pcb_t* proc) {

    if (proc == NULL) {
        return;
    }
    pcb_PTR deque[MAXPROC];
    int sp = 0, visited = 0;

    deque[sp++] = proc;

    while (sp != visited) {
        pcb_PTR curr = deque[visited++];
        // aggiunta figli

        if (emptyChild(curr))
            continue;

        pcb_PTR fchild = container_of(curr->p_child.next, pcb_t, p_child);
        pcb_PTR lchild = container_of(curr->p_child.prev, pcb_t, p_child);
        deque[sp++] = fchild;

        while (lchild != deque[sp - 1]) {
            struct list_head* next_sib = deque[sp - 1]->p_sib.next;
            deque[sp++] = container_of(next_sib, pcb_t, p_sib);
        }
    }

    for (int i = sp - 1; i >= 0; i--) {
        if (outBlocked(deque[i]))
            soft_block_count--;
        else
            outProcQ(&ready_queue, deque[i]);
        outChild(deque[i]);
        freePcb(deque[i]);
        process_count--;
    }
}

static void _termProcess() {
    state_t* exc_state = GET_EXCEPTION_STATE_PTR(0);
    int pid = exc_state->reg_a1;

    pcb_PTR target_pcb = pid == 0 ? running_pcb : pcbByPID(pid);
    _term(target_pcb);
    scheduler();
}

static void _reusablePasseren(state_t* cpu_state, int* semAdd) {
    // The running process has to wait for the resource
    if (*semAdd > 0) {
        *semAdd = *semAdd - 1;
    } else {
        insertBlocked(semAdd, running_pcb);
        soft_block_count++;
        _copyState(cpu_state, &running_pcb->p_s);

        _updateTime(running_pcb);

        // The scheduler must know that there's no running process
        // so that it can dispatch a new one properly.
        // running_pcb = NULL;
        scheduler();
    }

    // If resources were available, there's no need to call a scheduler
}

/*  NOTE:
    Positive semaphore values mean available resources.
    Semaphore value set to zero means waiting processes or no available
    resources.
*/
static void _passeren() {
    state_t* cpu_state = GET_EXCEPTION_STATE_PTR(0);
    int* semAdd = (int*)cpu_state->reg_a1;

    _reusablePasseren(cpu_state, semAdd);
}

static void _verhogen() {
    memaddr* semAdd = (memaddr*)GET_EXCEPTION_STATE_PTR(0)->reg_a1;

    // If there are blocked processes, unblock one
    pcb_t* removed_pcb = removeBlocked((int*)semAdd);

    if (removed_pcb != NULL) {
        insertProcQ(&ready_queue, removed_pcb);
    } else {
        *semAdd = *semAdd + 1;
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

        memaddr* statusp = &terminal_address->transm_status;
        memaddr* commandp = &terminal_address->transm_command;

        // If the command's address is the receiver register, we
        // need to use the terminal input semaphore
        if (commandp != (unsigned int*)cpu_state->reg_a1) {
            // NOTE: view initial.h interrupt lines map
            semaphore_index += 8;

            commandp = &terminal_address->recv_command;
            statusp = &terminal_address->recv_status;
        }

        // We need to tell the terminal the command stored in reg_a2
        if (*statusp == READY || *statusp == CHARRECV) {
            *commandp = cpu_state->reg_a2;
        }
        // REVIEW:
        // 1. If terminal is not ready we shouldn't wait.
        //    Access should be regulated via a mutex, not busy waiting.
        //    See print in p2test.c
        // 2. In case of error we return the status word != ready
        // 3. Spec 6.5 "For character transmission and receipt, the status
        //    word, in addition to containing a device completion code, will
        //    also contain the character transmitted or received" ?
        //
        else {
            cpu_state->reg_a0 = *statusp;
            return;
        }
    } else {
        dtpreg_t* device_address =
            (dtpreg_t*)(START_DEVREG + (semaphore_index * sizeof(devreg_t)));

        if (device_address->status == READY) {
            device_address->command = cpu_state->reg_a2;
        } else {
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
    // NOTE: If we don't update time we get the cumulative time it has run minus
    // the time it has just run.
    // |-5s-| ... |-3s-| ... |- X ->
    // X := current time;
    // p_time = 5s + 3s; // Nb. Not updated!
    _updateTime(running_pcb);
    // Now we need to reset the start time
    // why? Otherwise on the next _updateTime it will be like this:
    // p_time = 5s + 3s + 2s <- because of out last _updateTime
    // |-5s-| ... |-3s-| ... |- 8s -|X->
    // p_time = 5s + 3s + 2s + 8s // we count those 2s twice!!
    //
    // Everytime we _getCPUTIME we make a adjacent segment in the proc time
    // |-5s-| ... |-3s-| ... |-2s-||-6s-|X->
    //                            ^ update here
    // p_time = 5s+3s+2s+6s; // ok!
    STCK(start_time);
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
    if (list_empty(&ready_queue)) {
        return;
    }

    _copyState(GET_EXCEPTION_STATE_PTR(0), &running_pcb->p_s);

    // NOTE: we don't use insertChild() here because of spec 6.10:
    // "the yielded process is not immediately re-executed even
    // if it has the highest priority"
    pcb_t* first = removeProcQ(&ready_queue);
    insertProcQ(&ready_queue, running_pcb);
    // HACK: we override ProcQ functions
    if (first != NULL) {
        list_add(&first->p_list, &ready_queue);
    }

    _updateTime(running_pcb);

    // Call a ready process
    scheduler();
}
