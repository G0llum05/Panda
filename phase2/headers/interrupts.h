#ifndef INTERRUPTS_H
#define INTERRUPTS_H

static void interruptHandler();
static void nonTimerInterrupt(int IntlineNo);
static void localTimerInterrupt();

#endif
