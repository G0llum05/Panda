#include "./headers/asl.h"
#include "headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

// Inserts semaphore in the free semaphores list.
void freeSemd(semd_t* s) { list_add(&s->s_link, &semdFree_h); }

semd_t* allocSemd() {
    // Check availability for a new free semaphore.
    if (list_empty(&semdFree_h))
        return NULL;

    struct list_head* new_free_node = semdFree_h.next;
    list_del(new_free_node);

    semd_t* allocated_semd = container_of(new_free_node, semd_t, s_link);

    return allocated_semd;
}

/**
    Function to check if a semaphore is active.
    @return 1 if active, 0 if not.
*/
int semdIsActive(int* semAdd) {
    struct semd_t* current;
    list_for_each_entry(current, &semd_h, s_link) {
        // Cycles in the ASL to check for semaphore with key semAdd.
        if (current->s_key == semAdd)
            return TRUE;
    }
    return FALSE;
}

/**
    Given a semaphore address, it returns the pointer to the semaphore with
    key equal to semAdd.
    @return The pointer to the semaphore, else returns NULL.
*/
semd_t* getSemd(int* semAdd) {
    struct semd_t* current;
    list_for_each_entry(current, &semd_h, s_link) {
        if (current->s_key == semAdd)
            return current;
    }
    return NULL;
}

// Deactivates the semaphore and puts it in the free semaphores list.
void deactivateSemd(semd_t* semaphore) {
    list_del(&(semaphore->s_link));
    semaphore->s_key = NULL;
    freeSemd(semaphore);
}

void initASL() {
    INIT_LIST_HEAD(&semd_h);
    INIT_LIST_HEAD(&semdFree_h);
    for (int i = 0; i < MAXPROC; i++) {
        struct list_head* new_node = &semd_table[i].s_link;
        list_add(new_node, &semdFree_h);
    }
}

int insertBlocked(int* semAdd, pcb_t* p) {
    if (!semdIsActive(semAdd)) {
        // Semaphore doesn't exist
        semd_t* new_semd = allocSemd();
        if (new_semd == NULL)
            return TRUE;

        new_semd->s_key = semAdd;
        mkEmptyProcQ(&new_semd->s_procq);
        list_add_tail(&new_semd->s_link, &semd_h);
        list_add_tail(&p->p_list, &new_semd->s_procq);

    } else {
        // Semaphore already exists
        struct semd_t* current;
        current = getSemd(semAdd);
        list_add_tail(&p->p_list, &current->s_procq);
    }
    p->p_semAdd = semAdd;
    return FALSE;
}

pcb_t* removeBlocked(int* semAdd) {
    if (!semdIsActive(semAdd))
        return NULL;

    semd_t* semaphore = getSemd(semAdd);
    struct pcb_t* removedPCB = removeProcQ(&semaphore->s_procq);

    if (emptyProcQ(&semaphore->s_procq)) {
        deactivateSemd(semaphore);
    }
    return removedPCB;
}

pcb_t* outBlocked(pcb_t* p) {
    semd_t* semaphore = getSemd(p->p_semAdd);

    // Check integrity of semaphore pointer
    if (semaphore == NULL)
        return NULL;

    struct list_head* pos;

    // Cycles in the semaphore's process queue to remove p
    list_for_each(pos, &semaphore->s_procq) {
        pcb_t* pcb_node = container_of(pos, pcb_t, p_list);
        if (pcb_node == p) {
            list_del(pos);
            // Deactives semaphore if its process queue is empty
            if (emptyProcQ(&semaphore->s_procq)) {
                deactivateSemd(semaphore);
            }
            return pcb_node;
        }
    }
    return NULL;
}

pcb_t* headBlocked(int* semAdd) {
    if (!semdIsActive(semAdd))
        return NULL;

    semd_t* semaphore = getSemd(semAdd);
    return headProcQ(&semaphore->s_procq);
}
