#include "utils.h"
#include "string.h"

int simple_atoi(const char* s) {
    int res = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

void simple_itoa(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0'; buf[1] = '\0'; return;
    }
    int i = 0;
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    while (val != 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    if (is_neg) buf[i++] = '-';
    buf[i] = '\0';
    // Reverse string
    for(int j=0; j<i/2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i-j-1];
        buf[i-j-1] = tmp;
    }
}