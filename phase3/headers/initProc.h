#ifndef INITPROC_H
#define INITPROC_H

#define TERMINAL 1
#define TERMINALDEVICES TERMINAL * 2
#define FLASHDEVICES 8

extern unsigned int device_mutex[FLASHDEVICES + TERMINALDEVICES];
extern unsigned int shell_mutex;

#endif