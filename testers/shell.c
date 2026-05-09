#include "h/print.h"
#include "h/tconst.h"
#include <uriscv/liburiscv.h>

void main() {
    print(WRITETERMINAL, "ciao");
    SYSCALL(TERMINATE, 0, 0, 0);
}
