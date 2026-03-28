#include "string.h"
#include <stddef.h>
#include <stdint.h>

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

int strncmp(const char* s1, const char* s2, int n) {
    while (n > 0 && *s1 && (*s1 == *s2)) {
        s1++; s2++; n--;
    }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}

int strlen(const char* str) {
    int len = 0;
    while (str[len])
        len++;
    return len;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        // Handle overlapping memory by copying from the end
        unsigned char *lastd = d + (n-1);
        const unsigned char *lasts = s + (n-1);
        while (n--) *lastd-- = *lasts--;
    }
    return dest;
}

char* strcat(char* dest, const char* src) {
    char* tmp = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++));
    return tmp;
}

void* memcpy(void* dest, const void* src, size_t n) {
    // Use 32-bit copies for speed. Assumes n is a multiple of 4.
    uint32_t* d = dest;
    const uint32_t* s = src;
    for (size_t i = 0; i < n / 4; i++) d[i] = s[i];
    return dest;
}

void* memset(void* dest, int val, size_t n) {
    unsigned char* temp = (unsigned char*)dest;
    for ( ; n != 0; n--) *temp++ = val;
    return dest;
}