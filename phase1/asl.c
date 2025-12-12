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

int insertBlocked(int* semAdd, pcb_t* p) {
}

pcb_t* removeBlocked(int* semAdd) {
}

pcb_t* outBlocked(pcb_t* p) {
}

pcb_t* headBlocked(int* semAdd) {
}
