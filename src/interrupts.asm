; Add a note to prevent the linker from complaining about an executable stack.
section .note.GNU-stack noalloc noexec nowrite progbits

section .text

; Extern C functions to be called
extern keyboard_handler
extern mouse_handler

; Exported assembly handlers
global irq_common_stub
global keyboard_asm_handler
global mouse_asm_handler

; A generic stub for any unexpected interrupts.
irq_common_stub:
    pusha
    mov al, 0x20
    out 0xA0, al ; EOI to slave
    out 0x20, al ; EOI to master
    popa
    iret

keyboard_asm_handler:
    pusha
    call keyboard_handler
    mov al, 0x20
    out 0x20, al ; EOI to master PIC
    popa
    iret

mouse_asm_handler:
    pusha
    call mouse_handler
    mov al, 0x20
    out 0xA0, al ; EOI to slave PIC
    out 0x20, al ; EOI to master PIC
    popa
    iret