#ifndef VMSUPPORT_H
#define VMSUPPORT_H

// NOTE: Bits are zero-indexed
#define SETBITOFF(variable, bit) (variable = (variable) & ~(1 << (bit)))
#define SETBITON(variable, bit) (variable = (variable) | (1 << (bit)))

#define SWAPPOOLSIZE (2 * UPROCMAX)

// This bit is set to zero if the TLBP was a success
#define PROBEBIT 0x80000000

// Initializes swap table structures
void initSwapStructs();

// Handles all TLB-related exceptions
void pager();

#endif