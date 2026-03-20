#ifndef SHARED_H_
#define SHARED_H_

#include <uriscv/types.h>

void _copyState(state_t* src, state_t* dest);
void _exit();

#endif // SHARED_H_
