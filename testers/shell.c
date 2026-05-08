#include "h/string.h"
#include "h/tconst.h"
#include <uriscv/liburiscv.h>

// NOTE: first implemented function MUST be main
static void _exit();
static void _unknown();

#define SML 3

void main() {
    const char fibEight[] = "fibEight";
    const char echo[] = "echo";
    const char fibEleven[] = "fibEleven";
    const char uname[] = "uname";
    const char date[] = "date";
    const char sl[] = "sl";
    const char calc[] = "calc";
    const char exit[] = "exit";
    const char* commands[7] = {fibEight, echo, fibEleven, uname,
                               date,     sl,   calc};

    char ibuffer[128];
    char shell_mark[SML] = ">> ";
    int status = SYSCALL(WRITETERMINAL, (int)shell_mark, SML, 0);
    if (status < 0)
        _exit();

    while (1) {
        const int ilen = SYSCALL(READTERMINAL, (int)ibuffer, 0, 0);
        if (ilen < 0)
            _exit();

        char token[ilen];
        int found = 0;
        parse_word(ibuffer, token, ilen);

        for (int i = 0; i < 7; i++) {
            if (strcmp(token, commands[i]) == 0) {
                found = 1;
                SYSCALL(EXECUTE, i + 2, 0, 0);
            }
        }

        if (strcmp(token, exit) == 0) {
            _exit();
        }

        if (!found)
            _unknown();

        status = SYSCALL(WRITETERMINAL, (int)shell_mark, SML, 0);
        if (status < 0)
            _exit();
    }
}
static void _exit() { SYSCALL(TERMINATE, 0, 0, 0); }

static void _unknown() {
    // FIXME: can't print this shit, out of memory?
    /* const char unknown[] = "Unknown instruction\n"; */
    /* int status = SYSCALL(WRITETERMINAL, (int)unknown, 20, 0); */
    /* if (status < 0) */
    /*     _exit(); */
}
