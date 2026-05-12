#include "h/print.h"
#include "h/string.h"
#include "h/tconst.h"
#include <uriscv/liburiscv.h>

void main() {
    const char* commands[7] = {"fibEight", "echo", "fibEleven", "uname",
                               "date",     "sl",   "calc"};
    char ibuffer[128];
    char* shell_mark = ">> ";
    print(WRITETERMINAL, shell_mark);

    while (1) {
        const int ilen = SYSCALL(READTERMINAL, (int)ibuffer, 0, 0);
        if (ilen < 0)
            SYSCALL(TERMINATE, 0, 0, 0);

        char token[ilen];
        int found = 0;
        parse_word(ibuffer, token, ilen);

        for (int i = 0; i < 7; i++) {
            if (strcmp(token, commands[i]) == 0) {
                found = 1;
                SYSCALL(EXECUTE, i + 2, 0, 0);
                break;
            }
        }

        if (strcmp(token, "exit") == 0) {
            print(WRITETERMINAL, "Goodbye, and thanks for all the fish!\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }

        if (!found)
            print(WRITETERMINAL, "Unknown instruction.\n");
        print(WRITETERMINAL, shell_mark);
    }
    SYSCALL(TERMINATE, 0, 0, 0);
}
