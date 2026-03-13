/* Simple Linker Script for MinOS */
ENTRY(_start)

SECTIONS {
    /* Load the kernel at 1MB */
    . = 1M;

    .text : {
        *(.multiboot) /* The multiboot header must be first */
        *(.text)
    }

    .rodata : { *(.rodata) }

    /* Align data to a page boundary to create a new, writable segment */
    . = ALIGN(4096);
    .data   : { *(.data) }

    /* The BSS section for uninitialized static variables (like your backbuffer) */
    .bss : {
        *(COMMON)
        *(.bss)
    }
}