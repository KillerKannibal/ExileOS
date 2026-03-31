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
    ; Address fields (Offsets 12-28). Even though bit 16 is not set, 
    ; placeholders are required so that graphics fields reach offset 32.
    dd 0 ; header_addr
    dd 0 ; load_addr
    dd 0 ; load_end_addr
    dd 0 ; bss_end_addr
    dd 0 ; entry_addr
    ; Graphics fields (Offset 32)
    dd 0             ; mode_type (0 for linear framebuffer)
    dd 800           ; width
    dd 600           ; height
    dd 32            ; bpp

; Add a note to prevent the linker from complaining about an executable stack.
section .note.GNU-stack noalloc noexec nowrite progbits

section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB stack
stack_top:

section .text
global _start
global gdt_flush
global idt_load

extern kernel_main
extern sbss
extern ebss

_start:
    ; Quick "Hacker" status print to VGA Text Buffer (0xB8000)
    ; This writes 'E' 'X' 'I' 'L' 'E' to the top left of the screen
    mov dword [0xb8000], 0x074c0745 ; 'E' and 'L' (with grey attribute)
    
    ; 1. Clear BSS
    mov edi, sbss
    
    ; 1. Clear BSS before we use the stack or any static variables
    ; This prevents overwriting our own stack/arguments later.
    mov edi, sbss
    mov ecx, ebss
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; 2. Initialize our own stack
    mov esp, stack_top

    ; 3. Pass the multiboot info pointer (in ebx) to the C kernel.
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
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush ; Kernel Code Segment
.flush:
    mov ax, 0x2B    ; TSS index 5 (5 * 8 = 40 = 0x28) | RPL 3
    ltr ax
    ret

idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret