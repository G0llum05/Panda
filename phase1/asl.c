#include "./headers/asl.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    for(int i = 0; i < MAXPROC; i++) {
        struct list_head *new_node = &semd_table[i].s_link;
        list_add(new_node, &semdFree_h);
    }
}

/*
La funzione ha tre fasi:
    1. controllo se il semaforo è già attivo.
    2. se non è attivo lo prendo dai liberi.
    3. aggiorno i valori.
*/
int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *current_semaphore = NULL;                                   // Il semaforo che utilizziamo per bloccare il processo
    struct list_head *pos;                                              // Puntatore di scorrimento sulla lista
    
    list_for_each(pos, semd_h) {                                        // Controllo se il semaforo è già nella ASL
        semd_t *current_container = container_of(pos, semd_t, s_link); 
        if (current_container->s_key == semAdd) {
            current_semaphore = current_container;
            break;                                                      // Se lo trovo continuo
        }
    }
    if (current_semaphore == NULL) {                                    // Se non è nella ASL va allocato
        if (list_empty(semdFree_h)) return 1;                           // L'ASL è piena, quindi ritorniamo TRUE

        struct list_head *new_free_semaphore = list_next(semdFree_h);   // Prendo il primo libero
        list_del(new_free_semaphore);                                   // Non è più nella semdFree

        current_semaphore = container_of(new_free_semaphore, semd_t, s_link); // Il semaforo da utilizzare è il nuovo

        current_semaphore->s_key = semAdd;                              // Aggiorno la chiave del semaforo
        list_add_tail(&(current_semaphore->s_link), semd_h);            // Lo aggiungo come ultimo nuovo semaforo in arrivo
        mkEmptyProcQ(&(current_semaphore->s_procq));                    // Creo una nuova coda dei processi
    }
    
    insertProcQ(current_semaphore->s_procq, p);                         // Aggiungo p alla coda dei processi
    p->p_semAdd = semAdd;                                               // Aggiorno il semaforo a cui p è associato

    return 0;
}

pcb_t* removeBlocked(int* semAdd) {
}

pcb_t* outBlocked(pcb_t* p) {
}

pcb_t* headBlocked(int* semAdd) {
}
