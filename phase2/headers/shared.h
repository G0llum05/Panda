#ifndef SHARED_H_
#define SHARED_H_

#include "../../headers/types.h"
#include <uriscv/types.h>

void _copyState(state_t* src, state_t* dest);
void _exit();
void _updateTime(pcb_PTR p);

#endif // SHARED_H_
