#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

// I/O Port for COM1
#define COM1 0x3F8

// Initialize the serial port
void debug_init();

// Print a string to serial
void kprint_serial(const char* str);

// Print a 32-bit hex value (e.g., 0xDEADBEEF)
void kprint_hex(uint32_t n);

// Print a single character
void write_serial(char a);

#endif