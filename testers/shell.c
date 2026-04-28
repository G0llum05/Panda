#include <uriscv/liburiscv.h>

#include "h/string.h"
#include "h/tconst.h"

static void _exit() { SYSCALL(TERMINATE, 0, 0, 0); }

// commands procedures
static void _uname() { SYSCALL(EXECUTE, 4, 0, 0); }

typedef struct {
    char* token;
    void (*proc)(void);
} command_t;

// command mappings
const command_t commands[] = {{"uname", _uname}, {"exit", _exit}};

void main() {
    char ibuffer[256];
    while (1) { // main loop
        // read input
        const int ilen = SYSCALL(READTERMINAL, (int)ibuffer, 0, 0);
        if (ilen < 0)
            _exit(); // failed to read ?
        char token[ilen];
        char* it = (char*)parse_word(ibuffer, token, ilen);
        for (int i = 0; i < sizeof(commands) / sizeof(command_t); i++) {
            if (strcmp(token, commands[i].token) == 0) {
                // found token
                (*commands[i].proc)();
            }
        }

        // If here invalid token
        char* message = "FUCK YOU\n";
        if (SYSCALL(WRITETERMINAL, (int)message, strlen(message), 0) < 0) {
            _exit();
        }
        continue;
    }
    _exit();
}
