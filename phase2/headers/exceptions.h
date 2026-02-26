#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

// Handles TLB-refill events exceptions.
void uTLB_RefillHandler();

// Handles all exceptions, exclusive of TLB-Refill events.
void exceptionHandler();

#endif
