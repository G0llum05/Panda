#ifndef STRING_H
#define STRING_H

// defs
int strlen(const char* s);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
int strcmp(const char* a, const char* b);
const char* strchr(const char* s, char c);

int strlen(const char* s) {
    int len = 0;
    while (s[len])
        ++len;
    return len;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++))
        ;
}

void strcat(char* dest, const char* src) {
    while (*dest)
        ++dest;
    while ((*dest++ = *src++))
        ;
}

int strcmp(const char* a, const char* b) {
    while (*a && (*a == *b)) {
        ++a;
        ++b;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
const char* strchr(const char* s, char c) {
    while (*s) {
        if (*s == c)
            return s;
        ++s;
    }
    return 0;
}

// Helper: Skip leading whitespace
const char* skip_whitespace(const char* s) {
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'))
        ++s;
    return s;
}

// Helper: Parse integer from string, returns pointer to next char after number
const char* parse_int(const char* s, int* out_val) {
    int sign = 1, val = 0;
    s = skip_whitespace(s);
    if (*s == '-') {
        sign = -1;
        ++s;
    } else if (*s == '+') {
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        ++s;
    }
    *out_val = val * sign;
    return s;
}

// Helper: Copy non-space word, returns pointer to next char after word
const char* parse_word(const char* s, char* dest, int maxlen) {
    s = skip_whitespace(s);
    int i = 0;
    while (*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' &&
           i < maxlen - 1) {
        dest[i++] = *s++;
    }
    dest[i] = 0;
    return s;
}
#endif
