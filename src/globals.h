#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>
#include "multiboot.h"
#include "wm.h"

// --- Kernel Information Constants ---
#define KERNEL_NAME "ExileOS"
#define KERNEL_VERSION "0.2"
#define KERNEL_ARCH "x86"
#define KERNEL_VENDOR "KillerKannibal"

// Global Multiboot Info Pointer
extern struct multiboot_info* global_mbinfo;

// Start Menu State
extern int start_menu_open;
extern int wallpaper_mode; // 0=Grad Blue, 1=Solid Blue, 2=Grad Sunset, 3=Solid Gray
extern int theme_mode;     // 0=Dark, 1=Light, 2=Retro Blue
extern char cwd[128];    // Current Working Directory

// Mouse cursor
extern int mouse_on_resize_handle;

// Calc globals
extern char calc_buffer[64];
extern int calc_len;
extern char calc_result[64];

#endif