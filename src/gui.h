#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "multiboot.h"

// Graphics Globals (defined in gui.c)
extern uint32_t* lfb;
extern uint32_t screen_width;
extern uint32_t screen_height;
extern uint32_t screen_pitch;

// Function Declarations
void init_vga(struct multiboot_info* mbinfo);
void draw_pixel(uint32_t x, uint32_t y, uint32_t color);
void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void draw_char(char c, int x, int y, uint32_t color);
void draw_string(const char* str, int x, int y, uint32_t color);
void screen_update();
void draw_desktop(int wallpaper_mode);

#endif