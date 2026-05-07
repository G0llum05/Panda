#ifndef INITPROC_H
#define INITPROC_H

#define TERMINAL 1
#define TERMINALDEVICES TERMINAL * 2
#define FLASHDEVICES 8

extern unsigned int shell_mutex;
extern unsigned int master_semaphore;
void trigger_mutex(int code, int index);

#endif
