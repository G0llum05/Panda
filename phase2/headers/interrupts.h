#ifndef INTERRUPTS_H
#define INTERRUPTS_H

void interruptHandler();
static void nonTimerInterrupt(int intlineNo);
static void localTimerInterrupt();
static void pseudoClockTick();

#endif
