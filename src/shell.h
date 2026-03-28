#ifndef SHELL_H
#define SHELL_H

#include <stdint.h>

#define MAX_TERM_LINES 20
#define MAX_TERM_COLS  70

extern char term_buffer[MAX_TERM_LINES][MAX_TERM_COLS];
extern char keyboard_buffer[256];
extern int kb_buffer_len;

void term_scroll();
void term_print(const char* msg);
void exec_command(char* input);

#endif