# Simple Makefile for MinOS

# Toolchain
CC = gcc
AS = nasm

# Build flags
CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -Isrc
ASFLAGS = -f elf32

# Project files
OBJS = build/boot.o build/interrupts.o build/kernel.o build/gdt.o build/idt.o build/fs.o build/string.o build/gui.o
KERNEL = build/myos.bin
INITRD = build/initrd.tar
ISO = MinOS.iso

.PHONY: all run clean

all: $(ISO)

$(ISO): $(KERNEL) $(INITRD) src/grub.cfg
	@echo "==> Creating ISO image..."
	@mkdir -p isodir/boot/grub
	@cp $(KERNEL) isodir/boot/myos.bin
	@cp $(INITRD) isodir/boot/initrd.tar
	@cp src/grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(ISO) isodir > /dev/null 2>&1

$(KERNEL): $(OBJS) src/linker.ld
	@ld -m elf_i386 -T src/linker.ld -o $(KERNEL) $(OBJS)

build/%.o: src/%.c src/gdt.h src/idt.h src/io.h src/multiboot.h src/font.h src/fs.h src/string.h src/gui.h
	@mkdir -p build
	@$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.asm
	@mkdir -p build
	@$(AS) $(ASFLAGS) $< -o $@

$(INITRD):
	@echo "==> Creating initrd..."
	@tar -cf $(INITRD) -C fs .

run: $(ISO)
	@qemu-system-i386 -cdrom $(ISO) -m 256M

clean:
	@rm -rf build isodir $(ISO) $(INITRD)