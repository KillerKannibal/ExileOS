#ifndef STRING_H
#define STRING_H
#include <stddef.h> // For size_t

int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, int n);
void strcpy(char* dest, const char* src);
int strlen(const char* str);
void *memmove(void *dest, const void *src, size_t n);
char* strcat(char* dest, const char* src);
void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int val, size_t n);

#endif