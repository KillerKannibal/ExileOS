#include "idt.h"
#include "io.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load(uint32_t);
extern void keyboard_asm_handler();
extern void irq_common_stub(); // New common stub for safety
extern void mouse_asm_handler();

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_lo = (base & 0xFFFF);
    idt[num].base_hi = (base >> 16) & 0xFFFF;
    idt[num].sel     = sel;
    idt[num].always0 = 0;
    idt[num].flags   = flags;
}

void init_idt() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base  = (uint32_t)&idt;

    // 1. Point everything to a safe stub first to prevent "Freak Outs"
    for(int i = 0; i < 256; i++) {
        idt_set_gate(i, (uint32_t)irq_common_stub, 0x08, 0x8E);
    }

    // 2. Remap the PIC
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    
    // Mask everything except Keyboard (IRQ 1) and Cascade (IRQ 2)
    outb(0x21, 0xF9); 
    // Mask everything except Mouse (IRQ 12, which is Slave IRQ 4)
    outb(0xA1, 0xEF);

    // 3. Set the real keyboard handler
    idt_set_gate(0x21, (uint32_t)keyboard_asm_handler, 0x08, 0x8E);
    idt_set_gate(44, (unsigned)mouse_asm_handler, 0x08, 0x8E);

    idt_load((uint32_t)&idtp);
}