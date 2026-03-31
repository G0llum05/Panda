#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcb_table[MAXPROC];
static int next_pid = 1;

// Memset to zero-initialize members in alloc
void* memset(void* str, int c, size_tt n) {
    unsigned char* ptr = (unsigned char*)str;
    unsigned char value = (unsigned char)c;
    while (n-- > 0) {
        *ptr++ = value;
    }
    return str;
}

void initPcbs() {
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i = 0; i < MAXPROC; i++) {
        freePcb(&pcb_table[i]);
    }
}

void freePcb(pcb_t* p) { list_add(&p->p_list, &pcbFree_h); }

pcb_t* allocPcb() {
    if (list_empty(&pcbFree_h))
        return NULL;

    struct list_head* new_free_node = pcbFree_h.next;
    list_del(new_free_node);

    pcb_t* allocated_node = container_of(new_free_node, pcb_t, p_list);

    // Pcb tree fields
    allocated_node->p_parent = NULL;
    INIT_LIST_HEAD(&allocated_node->p_child);
    INIT_LIST_HEAD(&allocated_node->p_sib);

    // Process status information
    allocated_node->p_s = (state_t){0};
    allocated_node->p_time = 0;

    allocated_node->p_semAdd = NULL;

    allocated_node->p_supportStruct = NULL;

    allocated_node->p_prio = 0;

    allocated_node->p_pid = next_pid;
    next_pid++;

    return allocated_node;
}

void mkEmptyProcQ(struct list_head* head) { INIT_LIST_HEAD(head); }

int emptyProcQ(struct list_head* head) { return list_empty(head); }

void insertProcQ(struct list_head* head, pcb_t* p) {
    struct list_head* pos = head;

    list_for_each(pos, head) {
        pcb_t* pcb_node = container_of(pos, pcb_t, p_list);
        if (p->p_prio > pcb_node->p_prio) {
            list_add_tail(&p->p_list, pos);
            return;
        }
    }
    // If the node was not yet added, it has least priority
    list_add_tail(&p->p_list, head);
}

pcb_t* headProcQ(struct list_head* head) {
    if (list_next(head) == NULL)
        return NULL;
    return container_of(list_next(head), pcb_t, p_list);
}

pcb_t* removeProcQ(struct list_head* head) {
    struct list_head* node = list_next(head);
    if (node == NULL)
        return NULL;

    list_del(node);
    pcb_t* pcb_node = container_of(node, pcb_t, p_list);
    return pcb_node;
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    struct list_head* pos = head;
    list_for_each(pos, head) {
        pcb_t* pcb_node = container_of(pos, pcb_t, p_list);
        if (p == pcb_node) {
            struct list_head* prev_node = list_prev(&p->p_list);
            pcb_t* node = removeProcQ(prev_node);
            return node;
        }
    }

    return NULL;
}

int emptyChild(pcb_t* p) { return list_empty(&p->p_child); }

void insertChild(pcb_t* prnt, pcb_t* p) {
    if (emptyChild(prnt)) {
        prnt->p_child.prev = &p->p_child;
        prnt->p_child.next = &p->p_child;
        p->p_sib.prev = &prnt->p_sib;
        p->p_sib.next = &prnt->p_sib;
    } else {
        struct list_head* last_sib_ptr = prnt->p_child.prev;
        pcb_t* last_sib = container_of(last_sib_ptr, pcb_t, p_child);
        last_sib->p_sib.next = &p->p_sib;
        p->p_sib.prev = &last_sib->p_sib;
        p->p_sib.next = &prnt->p_sib;
        prnt->p_child.prev = &p->p_child;
    }
    p->p_parent = prnt;
}

pcb_t* removeChild(pcb_t* p) {
    if (emptyChild(p))
        return NULL;

    pcb_t* first_child_pcb = container_of(p->p_child.next, pcb_t, p_child);
    // this work only because list is not empty
    if (p->p_child.next == p->p_child.prev) {
        // only one child
        INIT_LIST_HEAD(&p->p_child);
    } else {
        pcb_t* second_child_pcb =
            container_of(first_child_pcb->p_sib.next, pcb_t, p_sib);
        p->p_child.next = &second_child_pcb->p_child;
        second_child_pcb->p_sib.prev = &p->p_sib;
    }

    first_child_pcb->p_parent = NULL;
    INIT_LIST_HEAD(&first_child_pcb->p_sib);
    return first_child_pcb;
}

pcb_t* outChild(pcb_t* p) {
    if (p->p_parent == NULL)
        return NULL;
    if (emptyChild(p->p_parent))
        return NULL;
    if (p->p_parent->p_child.next == &p->p_child) {
        // p is the first child
        return removeChild(p->p_parent);
    }
    if (p->p_parent->p_child.prev == &p->p_child) {
        // p is the last child
        p->p_parent->p_child.prev = p->p_sib.prev;
        pcb_t* prev_sib = container_of(p->p_sib.prev, pcb_t, p_sib);
        prev_sib->p_sib.next = &p->p_parent->p_sib;
        p->p_parent = NULL;
        INIT_LIST_HEAD(&p->p_sib);
        return p;
    }
    p->p_parent = NULL;
    list_del(&p->p_sib);
    return p;
}
