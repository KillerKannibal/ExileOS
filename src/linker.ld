/* ExileOS Linker Script - Forged for Thor */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS {
    /* Load the kernel at 1MB - Standard for Multiboot */
    . = 1M;

    .multiboot : ALIGN(4K) {
        *(.multiboot)
    }

    .text : ALIGN(4K) {
        *(.text)
    }

    /* Read-only data */
    .rodata BLOCK(4K) : ALIGN(4K) {
        *(.rodata)
    }

    /* Writable data (Page Aligned for Ring 0/3 separation later) */
    .data BLOCK(4K) : ALIGN(4K) {
        *(.data)
    }

    /* Uninitialized data (BSS) */
    .bss BLOCK(4K) : ALIGN(4K) {
        sbss = .;
        *(COMMON)
        *(.bss)
        ebss = .;
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment)
        *(.eh_frame)
    }
}