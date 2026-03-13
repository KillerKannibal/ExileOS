#include "gui.h"
#include "font.h"

// Graphics globals defined here
uint32_t* lfb;
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;


// The backbuffer is defined here. It's static so it's private to the GUI module.
static uint32_t backbuffer[1920 * 1080];

void init_vga(struct multiboot_info* mbinfo) {
    lfb = (uint32_t*)(uintptr_t)mbinfo->framebuffer_addr_lo;
    screen_width = mbinfo->framebuffer_width;
    screen_height = mbinfo->framebuffer_height;
    screen_pitch = mbinfo->framebuffer_pitch;
}

void draw_pixel(uint32_t x, uint32_t y, uint32_t color) {
    if (x >= screen_width || y >= screen_height) return;
    backbuffer[y * screen_width + x] = color;
}

void draw_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
    for (uint32_t i = 0; i < h; i++) {
        for (uint32_t j = 0; j < w; j++) {
            draw_pixel(x + j, y + i, color);
        }
    }
}

void draw_char(char c, int x, int y, uint32_t color) {
    if ((unsigned)c >= 128) return; // Out of font range
    const unsigned char* glyph = font[(int)c];
    for (int i = 0; i < 8; i++) { // 8 rows
        for (int j = 0; j < 8; j++) { // 8 bits/cols
            if ((glyph[i] >> (7 - j)) & 1) {
                draw_pixel(x + j, y + i, color);
            }
        }
    }
}

void draw_string(const char* str, int x, int y, uint32_t color) {
    while (*str) {
        draw_char(*str++, x, y, color);
        x += 8; // Advance to the next character position
    }
}

void screen_update() {
    uint8_t* lfb_row_ptr = (uint8_t*)lfb;
    uint32_t* backbuffer_ptr = backbuffer;

    for (uint32_t y = 0; y < screen_height; y++) {
        uint32_t* lfb_pixel_row = (uint32_t*)lfb_row_ptr;
        for (uint32_t x = 0; x < screen_width; x++) {
            lfb_pixel_row[x] = backbuffer_ptr[x];
        }
        lfb_row_ptr += screen_pitch;
        backbuffer_ptr += screen_width;
    }
}

void draw_desktop(int wallpaper_mode) {
    if (wallpaper_mode == 0) {
        // Nice vertical gradient background
        // Top: 0x151628 (Deep Dark Blue), Bottom: 0x243B55 (Slate Blue)
        for (uint32_t y = 0; y < screen_height; y++) {
            uint8_t r = 0x15 + (y * (0x24 - 0x15) / screen_height);
            uint8_t g = 0x16 + (y * (0x3B - 0x16) / screen_height);
            uint8_t b = 0x28 + (y * (0x55 - 0x28) / screen_height);
            uint32_t color = (r << 16) | (g << 8) | b;
            
            uint32_t* row = backbuffer + (y * screen_width);
            for (uint32_t x = 0; x < screen_width; x++) {
                row[x] = color;
            }
        }
    } else {
        // Solid Color (Midnight Blue)
        for (uint32_t i = 0; i < screen_width * screen_height; i++) {
            backbuffer[i] = 0x2C3E50; 
        }
    }
}