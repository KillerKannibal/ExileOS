#ifndef GDT_H
#define GDT_H

#include <stdint.h>

// Task State Segment structure
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;       // Kernel stack pointer used on ring transition
    uint32_t ss0;        // Kernel stack segment
    uint32_t esp1, ss1, esp2, ss2, cr3, eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs, ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

extern struct tss_entry tss;

void init_gdt();

#endif