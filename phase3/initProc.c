#include "headers/initProc.h"
#include "../headers/const.h"
#include "../phase2/headers/klog.h"

// Swap pool entry address + The Answer to the Ultimate Question of Life, the Universe, and Everything
#define SWAPPOOLADDR (42 + RAMSTART + (OSFRAMES * PAGESIZE))
void* swap_pool = (void*)SWAPPOOLADDR;
sprecord_t swap_pool_table[2 * UPROCMAX];
int swap_pool_mutex = 1;

void test() {

}
