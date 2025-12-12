#include "./headers/pcb.h"
void klog_print(char *str);
void klog_print_dec(unsigned int *str);

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

char *int_to_string(int value, char *str) {
  int is_negative = 0;
  int i = 0; // Indice per il buffer str

  // CASO 1: Gestione dello Zero
  if (value == 0) {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // CASO 2: Gestione dei Negativi
  if (value < 0) {
    is_negative = 1;
    // Convertiamo il negativo in positivo per la divisione.
    // Usiamo un long per evitare overflow in caso di INT_MIN.
    value = (int)-value;
  }

  // CASO 3: Conversione della Cifra
  // Usiamo la divisione e il modulo per estrarre le cifre in ordine inverso
  while (value != 0) {
    int remainder = value % 10;
    // Converti la cifra in un carattere ASCII aggiungendo '0'
    str[i++] = remainder + '0';
    value = value / 10;
  }

  // Aggiungi il segno negativo se necessario
  if (is_negative) {
    str[i++] = '-';
  }

  // Termina la stringa con il carattere nullo
  str[i] = '\0';

  // Le cifre sono nel buffer, ma al contrario (es. 543 -> "345-")
  // Dobbiamo invertire la stringa.

  // Inversione della stringa
  int start = 0;
  int end = i - 1;
  char temp;

  while (start < end) {
    temp = str[start];
    str[start] = str[end];
    str[end] = temp;
    start++;
    end--;
  }

  return str;
}

void printList(struct list_head *head) {
  char str[200];
  struct list_head *pos = head;
  klog_print("[");
  list_for_each(pos, head) {
    int prio = container_of(pos, pcb_t, p_list)->p_prio;
    char *new_str = int_to_string(prio, str);
    klog_print(new_str);
    klog_print(", ");
  }
  klog_print("]");
  klog_print("\n");
}

void printNodeContent(struct pcb_t *p) {
  char str[200];
  klog_print("{");
  int prio = p->p_prio;
  char *new_str = int_to_string(prio, str);
  klog_print(new_str);
  klog_print(", ");
  klog_print("}");
}

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
  //
  // process status information
  allocated_node->p_s = (state_t){0};
  allocated_node->p_time = 0;

  allocated_node->p_semAdd = NULL;

  allocated_node->p_supportStruct = NULL;

  allocated_node->p_prio = 0;

  allocated_node->p_pid++;

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
