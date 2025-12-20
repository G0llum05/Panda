#include "./headers/pcb.h"

void *memset(void *str, int c, size_tt n) {
  unsigned char *ptr = (unsigned char *)str;
  unsigned char value = (unsigned char)c;
  while (n-- > 0) {
    *ptr++ = value;
  }
  return str;
}

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
  INIT_LIST_HEAD(&pcbFree_h);
  for (int i = 0; i < MAXPROC; i++) {
    struct list_head *new_node = &pcbFree_table[i].p_list;
    list_add(new_node, &pcbFree_h);
  }
}

void freePcb(pcb_t *p) { list_add(&p->p_list, &pcbFree_h); }

pcb_t *allocPcb() {
  if (list_empty(&pcbFree_h)) {
    return NULL;
  }
  struct list_head *new_free_node = pcbFree_h.next;
  list_del(new_free_node);

  struct pcb_t *allocated_node = container_of(new_free_node, pcb_t, p_list);

  // pcb tree fields
  allocated_node->p_parent = NULL;
  INIT_LIST_HEAD(&allocated_node->p_child);
  INIT_LIST_HEAD(&allocated_node->p_sib);

  // process status information
  allocated_node->p_s = (state_t){0};
  allocated_node->p_time = 0;

  allocated_node->p_semAdd = NULL;

  allocated_node->p_supportStruct = NULL;

  allocated_node->p_prio = 0;

  allocated_node->p_pid = next_pid;
  next_pid++;

  return allocated_node;
}

void mkEmptyProcQ(struct list_head *head) { INIT_LIST_HEAD(head); }

int emptyProcQ(struct list_head *head) { return list_empty(head); }

void insertProcQ(struct list_head *head, pcb_t *p) {
  struct list_head *pos = head;

  list_for_each(pos, head) {
    struct pcb_t *pcb_node = container_of(pos, pcb_t, p_list);
    if (p->p_prio > pcb_node->p_prio) {
      list_add_tail(&p->p_list, pos);
      return;
    }
  }
  // Se non è stato inserito il nodo da inserire è quello con la priorità
  // minore
  list_add_tail(&p->p_list, head);
}

pcb_t *headProcQ(struct list_head *head) {
  if (list_next(head) == NULL)
    return NULL;
  return container_of(list_next(head), pcb_t, p_list);
}

pcb_t *removeProcQ(struct list_head *head) {
  struct list_head *node = list_next(head);
  if (node == NULL)
    return NULL;
  list_del(node);
  struct pcb_t *pcb_node = container_of(node, pcb_t, p_list);
  return pcb_node;
}

pcb_t *outProcQ(struct list_head *head, pcb_t *p) {
  struct list_head *pos = head;
  list_for_each(pos, head) {
    struct pcb_t *pcb_node = container_of(pos, pcb_t, p_list);
    if (p == pcb_node) {
      // se due nodi sono uguali vuol dire che c'è sempre un nodo prev
      struct list_head *prev_node = list_prev(&p->p_list);
      struct pcb_t *node = removeProcQ(prev_node);
      return node;
      // return removeProcQ(prev_node);
    }
  }
  // Se non è stato rimosso il nodo significa che non era presente il nodo p
  // nella lista dei nodi
  return NULL;
}

int emptyChild(pcb_t *p) { return list_empty(&p->p_child); }

void insertChild(pcb_t *prnt, pcb_t *p) {
  list_add_tail(&p->p_child, &prnt->p_child);
  p->p_parent = prnt;
}

pcb_t *removeChild(pcb_t *p) {
  if (list_empty(&p->p_child))
    return NULL;
  struct list_head *new_free_node = p->p_child.next;
  list_del(new_free_node);
  struct pcb_t *child_pcb = container_of(new_free_node, pcb_t, p_child);
  child_pcb->p_parent = NULL;
  return child_pcb;
}

pcb_t *outChild(pcb_t *p) {
  if (p->p_parent == NULL)
    return NULL;
  struct pcb_t *parent = p->p_parent;
  struct list_head *parent_child = &parent->p_child;
  // c'è almeno un nodo
  struct list_head *pos = parent_child->next;

  list_for_each(pos, parent_child) {
    struct pcb_t *pcb_node = container_of(pos, pcb_t, p_child);
    if (p == pcb_node) {
      list_del(pos);
      p->p_parent = NULL;
      return p;
    }
  }
  // Se non è stato trovato
  return NULL;
}
