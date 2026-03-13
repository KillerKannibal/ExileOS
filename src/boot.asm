; Multiboot header constants
MBALIGN  equ  1<<0
MEMINFO  equ  1<<1
VIDINFO  equ  1<<2
MAGIC    equ  0x1BADB002
FLAGS    equ  MBALIGN | MEMINFO | VIDINFO
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; Graphics fields to match grub.cfg
    dd 0, 0, 0, 0, 0 ; header/load addresses
    dd 0             ; mode_type (0 for linear framebuffer)
    dd 1920          ; width
    dd 1080          ; height
    dd 32            ; bpp

; Add a note to prevent the linker from complaining about an executable stack.
section .note.GNU-stack noalloc noexec nowrite progbits

section .text
global _start
global gdt_flush
global idt_load

extern kernel_main

_start:
    ; GRUB has set up Protected Mode and a stack.
    ; We pass the multiboot info pointer (in ebx) to the C kernel.
    push ebx
    call kernel_main

    ; Hang if kernel_main ever returns
    cli
.hang:
    hlt
    jmp .hang

gdt_flush:
    mov eax, [esp+4]
    lgdt [eax]
    mov ax, 0x10 ; Kernel Data Segment
    mov ds, ax; mov es, ax; mov fs, ax; mov gs, ax; mov ss, ax
    jmp 0x08:.flush ; Kernel Code Segment
.flush:
    ret

idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret