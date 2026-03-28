#include <stddef.h>
#include "globals.h"

// Global Multiboot Info Pointer
struct multiboot_info* global_mbinfo = NULL;

// Start Menu State
int start_menu_open = 0;
int wallpaper_mode = 0; // 0 = Gradient, 1 = Solid
int theme_mode = 0;     // 0 = Dark, 1 = Light, 2 = Retro
char cwd[128] = "/";    // Current Working Directory

// Mouse cursor
int mouse_on_resize_handle = 0;

// Calc globals
char calc_buffer[64];
int calc_len = 0;
char calc_result[64] = "0";