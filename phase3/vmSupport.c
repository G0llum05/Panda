#include "../headers/types.h"
#include <uriscv/liburiscv.h>

void uTLB_RefillHandler() {
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();
    LDST((state_t*)BIOSDATAPAGE);
}
