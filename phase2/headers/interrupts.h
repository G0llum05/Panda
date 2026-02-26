#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interruptHandler();
static void nonTimerInterrupt(int IntlineNo);
static void localTimerInterrupt();

#endif
