#ifndef INITPROC_H
#define INITPROC_H

typedef struct {
    union {
        unsigned int data;
        struct {
            unsigned int asid;
            unsigned int vpn : 4;
        } bit;
    } entryHi;
    union {
        unsigned int data;
        struct {
            unsigned int N : 1;
            unsigned int D : 2;
            unsigned int V : 3;
            unsigned int G : 4;
            unsigned int pfn;
        } bit;
    } entryLo;
} pgrecord_t;

// sprecord := Swap Pool table record
typedef struct sprecord {
    int ASID;
    int VPN;
    pgrecord_t* page;
} sprecord_t;

#endif