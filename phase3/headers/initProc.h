#ifndef INITPROC_H
#define INITPROC_H

#define TERMINAL 1
#define TERMINALDEVICES TERMINAL * 2
#define FLASHDEVICES 8

extern unsigned int shell_mutex;
extern unsigned int master_semaphore;
extern unsigned int support_mutex[FLASHDEVICES + TERMINALDEVICES];
extern unsigned int flashpoolstart;
#define FLASHPOOLSTART (flashpoolstart)
#endif
