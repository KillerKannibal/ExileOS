#ifndef IO_H
#define IO_H
#include <stdint.h>

/* Use "a" to force the 'al' register for the value */
#define outb(port, val) \
    asm volatile ("outb %b0, %w1" : : "a"((uint8_t)(val)), "Nd"((uint16_t)(port)))

#define inb(port) ({ \
    uint8_t _ret; \
    asm volatile ("inb %w1, %b0" : "=a"(_ret) : "Nd"((uint16_t)(port))); \
    _ret; \
})

#endif