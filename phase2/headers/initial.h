#ifndef INITIAL_H
#define INITIAL_H

#include "../../headers/types.h"

// Global variables
extern unsigned int process_count;
extern unsigned int soft_block_count;
extern pcb_PTR running_pcb;
extern struct list_head ready_queue;
extern void test();

/* Interrupt lines map:
   Line 3 (Disk)         = indices 0-7 
   Line 4 (Flash/Tape)   = indices 8-15
   Line 5 (Network)      = indices 16-23
   Line 6 (Printer)      = indices 24-31
   Line 7 (Term Output)  = indices 32-39
   Line 7 (Term Input)   = indices 40-47
*/
extern int device_semaphores[SEMDEVLEN];
extern int pseudo_clock_semaphore;

#endif
