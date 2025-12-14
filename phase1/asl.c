#include "./headers/asl.h"
#include "headers/pcb.h"
void mkEmptyProcQ(struct list_head *head);

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

void initSemdSentinel() { INIT_LIST_HEAD(&semd_h); }

void freeSemd(semd_t *s) { list_add(&s->s_link, &semdFree_h); }

semd_t *allocSemd() {
  if (list_empty(&semdFree_h)) {
    return NULL;
  }
  struct list_head *new_free_node = semdFree_h.next;
  list_del(new_free_node);

  semd_t *allocated_semd = container_of(new_free_node, semd_t, s_link);

  // TODO CHECK: reset datas?
  return allocated_semd;
}

int semdIsActive(int *semAdd) {
  struct semd_t *current;
  list_for_each_entry(current, &semd_h, s_link) {
    if (current->s_key == semAdd) {
      return TRUE;
    }
  }
  return FALSE;
}

semd_t *getSemd(int *semAdd) {
  struct semd_t *current;
  list_for_each_entry(current, &semd_h, s_link) {
    if (current->s_key == semAdd) {
      return current;
    }
  }
  return NULL;
}

void initASL() {
  initSemdSentinel();
  INIT_LIST_HEAD(&semdFree_h);
  for (int i = 0; i < MAXPROC; i++) {
    struct list_head *new_node = &semd_table[i].s_link;
    list_add(new_node, &semdFree_h);
  }
}

int insertBlocked(int *semAdd, pcb_t *p) {
  if (!semdIsActive(semAdd)) {
    // semaphore doesn't exists
    semd_t *new_semd = allocSemd();
    if (new_semd == NULL) {
      return TRUE;
    }

    new_semd->s_key = semAdd;
    mkEmptyProcQ(&new_semd->s_procq);
    list_add_tail(&new_semd->s_link, &semd_h);
    list_add_tail(&p->p_list, &new_semd->s_procq);

  } else {
    // semaphore already exists
    struct semd_t *current;
    current = getSemd(semAdd);
    // Direi che questo controllo non è necessario
    // if (current == NULL) {
    //   return TRUE;
    // }
    list_add_tail(&p->p_list, &current->s_procq);
  }
  p->p_semAdd = semAdd;
  return FALSE;
}

pcb_t *removeBlocked(int *semAdd) {
  if (!semdIsActive(semAdd)) {
    return NULL;
  }

  semd_t *semaphore = getSemd(semAdd);
  // Non dovrebbe servire questo controllo
  // if (semaphore == NULL) {
  //   return NULL;
  // }
  struct pcb_t *removedPCB = removeProcQ(&semaphore->s_procq);

  if (emptyProcQ(&semaphore->s_procq)) {
    list_del(&semaphore->s_link);
    semaphore->s_key = NULL;
    freeSemd(semaphore);
  }
  return removedPCB;
}

pcb_t *outBlocked(pcb_t *p) {
  if (p->p_semAdd == NULL) {
    return NULL;
  }

  semd_t *semaphore = getSemd(p->p_semAdd);
  // Potrebbe servire qua, es il semAdd punta a qualcosa di non valido
  if (semaphore == NULL) {
    return NULL;
  }

  struct list_head *pos;
  list_for_each(pos, &semaphore->s_procq) {
    pcb_t *pcb_node = container_of(pos, pcb_t, p_list);
    if (pcb_node == p) {
      list_del(pos);
      if (emptyProcQ(&semaphore->s_procq)) {
        list_del(&semaphore->s_link);
        semaphore->s_key = NULL;
        freeSemd(semaphore);
      }
      return pcb_node;
    }
  }
  return NULL;
}

pcb_t *headBlocked(int *semAdd) {
  if (!semdIsActive(semAdd)) {
    return NULL;
  }

  semd_t *semaphore = getSemd(semAdd);
  // Non dovrebbe servire questo controllo
  // if (semaphore == NULL) {
  //   return NULL;
  // }
  return headProcQ(&semaphore->s_procq);
}
