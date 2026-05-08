#ifndef STRING_H
#define STRING_H

// defs
int strlen(const char* s);
void strcpy(char* dest, const char* src);
void strcat(char* dest, const char* src);
int strcmp(const char* a, const char* b);
const char* strchr(const char* s, char c);
const char* parse_word(const char* s, char* dest, int maxlen);
const char* parse_int(const char* s, int* out_val);
const char* skip_whitespace(const char* s);
void* memcpy(void* dest, const void* src, unsigned int n);
void* memset(void* str, int c, unsigned int n);
#endif
