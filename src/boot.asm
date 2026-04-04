; Updated Diagnostic boot stub for ExileOS (64-bit transition)
MBALIGN  equ 1<<0
MEMINFO  equ 1<<1
VIDEO    equ 1<<2
MAGIC    equ 0x1BADB002
FLAGS    equ MBALIGN | MEMINFO | VIDEO
CHECKSUM equ -(MAGIC + FLAGS)

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM
    ; Required fields for VIDEO flag (set to 0 for ELF transparency)
    dd 0, 0, 0, 0, 0 
    dd 0             ; 0 = Linear graphics mode
    dd 800           ; Preferred Width
    dd 600           ; Preferred Height
    dd 32            ; Preferred Depth (Bits per pixel)

section .bss
align 16
    stack: resb 4096

align 4096
    pml4: resq 512
    pdpt: resq 512
    pdt:  resq 2048 ; Expanded to 4 tables to cover 4GB (512 * 2MB * 4)

section .text
bits 32
global _start
extern kernel_main
extern sbss
extern ebss

_start:
    ; 1. Diagnostic: Started
    mov al, '1'
    out 0xE9, al

    cli
    cld

    ; 2. Clear BSS
    mov edi, sbss
    mov ecx, ebss
    sub ecx, edi
    xor eax, eax
    rep stosb

    ; 3. Stack setup
    mov esp, stack + 4096

    ; 4. Paging Setup (Identity mapping 0MB to 4096MB via 2MB Huge Pages)
    ; PML4[0] -> PDPT[0]
    mov eax, pdpt
    or eax, 0x03 ; PRESENT | WRITABLE
    mov [pml4], eax

    ; PDPT[0,1,2,3] -> 4 separate Page Directory Tables
    ; This covers 4GB of physical address space
    mov eax, pdt
    or eax, 0x03
    mov [pdpt], eax      ; PDPT entry 0 (0-1GB)
    
    add eax, 4096
    mov [pdpt + 8], eax  ; PDPT entry 1 (1-2GB)
    
    add eax, 4096
    mov [pdpt + 16], eax ; PDPT entry 2 (2-3GB)
    
    add eax, 4096
    mov [pdpt + 24], eax ; PDPT entry 3 (3-4GB)

    ; Fill the PDTs with 2048 entries (4 * 512)
    mov edi, pdt
    mov eax, 0x00000183  ; PRESENT | WRITABLE | HUGE_PAGE
    mov ecx, 2048
.loop_paging:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000    ; Next 2MB page
    loop .loop_paging

    ; 5. Enable PAE
    mov eax, cr4
    or eax, 0x20
    mov cr4, eax

    ; 6. Load PML4
    lea eax, [pml4]
    mov cr3, eax

    ; 7. Enable Long Mode in EFER
    mov ecx, 0xC0000080
    rdmsr
    or eax, 0x100
    wrmsr

    ; 8. Enable Paging
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; 9. Diagnostic: Entering GDT/Long Mode
    mov al, '3'
    out 0xE9, al

    lgdt [gdt_ptr]

    ; Long jump to 64-bit code segment
    jmp 0x08:.lm

bits 64
.lm:
    ; Set up 64-bit data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    mov rsp, stack + 4096
    
    ; Pass Multiboot pointer (rbx) to kernel_main (rdi)
    mov rdi, rbx
    call kernel_main

    ; If kernel returns, halt
    cli
.h:
    hlt
    jmp .h

section .data
align 8
gdt:
    dq 0                         ; Null descriptor
    dq 0x00209A0000000000        ; 64-bit Code (Kernel)
    dq 0x0000920000000000        ; 64-bit Data (Kernel)
gdt_len equ $ - gdt

gdt_ptr:
    dw gdt_len - 1
    dq gdt

global idt_load
idt_load:
    lidt [rdi]
    ret