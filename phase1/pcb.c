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

int emptyChild(pcb_t* p) {
    pcb_t* next_child = container_of(p->p_child.next, pcb_t, p_child);
    return next_child->p_parent != p; // Not out child, list circled, emptyChild
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    if (emptyChild(prnt)) {
        list_add_tail(&p->p_child, &prnt->p_child);
    } else {
        struct list_head* node = prnt->p_child.next;
        pcb_t* child = container_of(node, pcb_t, p_child);
        list_add_tail(&p->p_sib, &child->p_sib);
    }
    p->p_parent = prnt;
    /* list_add_tail(&p->p_child, &prnt->p_child); */
    /* p->p_parent = prnt; */
}

pcb_t* removeChild(pcb_t* p) {
    if (emptyChild(p))
        return NULL;

    struct list_head* cnode = p->p_child.next; // to be deleted
    pcb_t* child_pcb = container_of(cnode, pcb_t, p_child);
    struct list_head* snode = child_pcb->p_sib.next;
    pcb_t* sibling_pcb = container_of(snode, pcb_t, p_sib);

    // detach child
    child_pcb->p_parent = NULL;
    p->p_child.next = &sibling_pcb->p_child;

    list_del(&child_pcb->p_sib); // remove it from siblings
    return child_pcb;
}

pcb_t* outChild(pcb_t* p) {
    if (p->p_parent == NULL)
        return NULL;
    if (p->p_parent->p_child.next == &p->p_child) {
        // first child
        return removeChild(p->p_parent);
    } else {
        list_del(&p->p_sib);
        p->p_parent = NULL;
        return p;
    }
}
