#include "h/tconst.h"
#include <uriscv/liburiscv.h>

#define DIALOG_LENGTH 22
#define MAX_INPUT 6
#define MAX_OUTPUT 3
typedef enum OP { ADD, SUB, MUL, DIV } op;

int stoe(char* input, int* first_number, int* second_number, op* operand);
void itos(char* output, int result);

void main() {
    const char input_dialog[DIALOG_LENGTH] = "Enter an expression: ";
    SYSCALL(WRITETERMINAL, (int)input_dialog, DIALOG_LENGTH, 0);

    char input[MAX_INPUT];
    char output[MAX_OUTPUT];
    int first_number, second_number, result;
    op operand;

    SYSCALL(READTERMINAL, (int)input, MAX_INPUT, 0);

    if (stoe(input, &first_number, &second_number, &operand) == -1) {
        const char error_dialog[DIALOG_LENGTH] = "Input error         ";
        SYSCALL(WRITETERMINAL, (int)error_dialog, DIALOG_LENGTH, 0);
        return;
    }

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
        result = (second_number != 0) ? first_number / second_number : 0;
        break;
    }

    itos(output, result);
    SYSCALL(WRITETERMINAL, (int)output, MAX_OUTPUT, 0);

    SYSCALL(TERMINATE, 0, 0, 0);
}

int stoe(char* buffer, int* first_number, int* second_number, op* operand) {
    char* p = buffer;

    if (*p < '0' || *p > '9')
        return -1;
    *first_number = *p - '0';
    p++;

    while (*p == ' ')
        p++;

    switch (*p) {
    case '+':
        *operand = ADD;
        break;
    case '-':
        *operand = SUB;
        break;
    case '*':
        *operand = MUL;
        break;
    case '/':
        *operand = DIV;
        break;
    default:
        return -1;
    }
    p++;

    while (*p == ' ')
        p++;

    if (*p < '0' || *p > '9')
        return -1;
    *second_number = *p - '0';

    return 0;
}

void itos(char* output, int result) {
    if (result < 0)
        result = 0;
    output[0] = (result / 10) + '0';
    output[1] = (result % 10) + '0';
    output[2] = '\0';
}