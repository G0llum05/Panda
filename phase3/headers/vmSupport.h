#ifndef VMSUPPORT_H
#define VMSUPPORT_H

#include "../../headers/types.h"

// NOTE: Bits are zero-indexed
#define SETBITOFF(variable, bit) (variable = (variable) & ~(1 << (bit)))
#define SETBITON(variable, bit) (variable = (variable) | (1 << (bit)))

#define SWAPPOOLSIZE (2 * UPROCMAX)

// This bit is set to zero if the TLBP was a success
#define PROBEBIT 0x80000000

#define SHELLASID 1
#define FIBEIGHTASID 2
#define ECHOASID 3
#define FIBELEVENASID 4
#define UNAMEASID 5
#define DATEASID 6
#define SLASID 7
#define CALCASID 8

unsigned int shell_mutex;
unsigned int master_semaphore;

// Small snippet of reusable state assignment logic
void setState(state_t* state, unsigned int asid);

// Initializes swap table structures
void initSwapStructs();

// Handles all TLB-related exceptions
void pager();

void initSupportPool();
support_t* allocSupportStruct();
void freeSupportStruct(support_t* sup);
int isSupportPoolEmpty();

#endif
