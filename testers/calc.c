#include "h/print.h"
#include "h/tconst.h"
#include <uriscv/liburiscv.h>

#define MAX_INPUT 16
#define MAX_OUTPUT 4
typedef enum OP { ADD, SUB, MUL, DIV } op;

// Simple calculator program. Divisions are rounded towards zero
void main() {
    char* input_dialog = "Enter an expression: ";
    print(WRITETERMINAL, input_dialog);

    char input[MAX_INPUT];
    char output[MAX_OUTPUT] = "00\n";
    int first_number, second_number, result;
    op operand;

    SYSCALL(READTERMINAL, (int)input, MAX_INPUT, 0);

    char* p = input;

    if (*p < '0' || *p > '9') {
        print(WRITETERMINAL, "Input error\n");
        return;
    }
    first_number = *p - '0';
    p++;

    while (*p == ' ')
        p++;

    switch (*p) {
    case '+':
        operand = ADD;
        break;
    case '-':
        operand = SUB;
        break;
    case '*':
        operand = MUL;
        break;
    case '/':
        operand = DIV;
        break;
    default:
        print(WRITETERMINAL, "Input error\n");
        return;
    }
    p++;

    while (*p == ' ')
        p++;

    if (*p < '0' || *p > '9') {
        print(WRITETERMINAL, "Input error\n");
        return;
    }
    second_number = *p - '0';

    switch (operand) {
    case ADD:
        result = first_number + second_number;
        break;
    case SUB:
        result = first_number - second_number;
        break;
    case MUL:
        result = first_number * second_number;
        break;
    case DIV:
        if (second_number != 0) {
            result = first_number / second_number;
        } else {
            print(WRITETERMINAL, "Invalid divisor\n");
            SYSCALL(TERMINATE, 0, 0, 0);
        }
        break;
    }

    if (result < 0)
        result = 0;
    output[0] = (result / 10) + '0';
    output[1] = (result % 10) + '0';

    print(WRITETERMINAL, output);

    SYSCALL(TERMINATE, 0, 0, 0);
}