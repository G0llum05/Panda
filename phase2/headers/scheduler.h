#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../../headers/types.h"

// This variable stores the cpu time at which the current process started
extern cpu_t start_time;

void scheduler();

#endif
