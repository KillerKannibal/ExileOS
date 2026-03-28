#include "gui.h"
#include "font.h"
#include "string.h"
#include "input.h"
#include "rtc.h"
#include "wm.h"
#include "globals.h"

// Graphics globals defined here
uint32_t* lfb;
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;


// The backbuffer is defined here. It's static so it's private to the GUI module.
static uint32_t backbuffer[800 * 600];
static uint32_t static_buffer[800 * 600];

void init_vga(struct multiboot_info* mbinfo) {
    // CRITICAL SAFETY CHECK: Prevent buffer overflows
    // If resolution is higher than our arrays, we must stop before corrupting memory.
    if (mbinfo->framebuffer_width > 800 || mbinfo->framebuffer_height > 600) {
        lfb = (uint32_t*)(uintptr_t)mbinfo->framebuffer_addr_lo;
        for (uint32_t i = 0; i < 10000; i++) lfb[i] = 0xFFFF00; // Yellow warning strip
        while(1);
    }

    lfb = (uint32_t*)(uintptr_t)mbinfo->framebuffer_addr_lo;
    screen_width = mbinfo->framebuffer_width;
    screen_height = mbinfo->framebuffer_height;
    screen_pitch = mbinfo->framebuffer_pitch;

    // If the bootloader gave us 24-bit or 16-bit, our 32-bit driver will look "broken"
    // We fill the screen with RED to indicate a BPP error instead of just hanging silently
    if (mbinfo->framebuffer_bpp != 32) {
        for (uint32_t i = 0; i < screen_width * screen_height; i++) lfb[i] = 0xFF0000;
        while(1); 
    }

    // Clear the real screen to black immediately
    for (uint32_t i = 0; i < screen_width * screen_height; i++) {
        lfb[i] = 0x000000; 
    }
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
    // This is safer and faster than a single memcpy if pitch != width*4
    uint8_t* lfb_ptr = (uint8_t*)lfb;
    uint32_t* backbuffer_ptr = backbuffer;

    for (uint32_t y = 0; y < screen_height; y++) {
        memcpy(lfb_ptr, backbuffer_ptr, screen_width * 4);
        lfb_ptr += screen_pitch;
        backbuffer_ptr += screen_width;
    }
}

void init_desktop_buffer(int wallpaper_mode) {
    if (wallpaper_mode == 0) { // Grad Blue
        for (uint32_t y = 0; y < screen_height; y++) {
            uint8_t r = 0x15 + (y * (0x24 - 0x15) / screen_height);
            uint8_t g = 0x16 + (y * (0x3B - 0x16) / screen_height);
            uint8_t b = 0x28 + (y * (0x55 - 0x28) / screen_height);
            uint32_t color = (r << 16) | (g << 8) | b;
            for (uint32_t x = 0; x < screen_width; x++) static_buffer[y * screen_width + x] = color;
        }
    } else if (wallpaper_mode == 1) { // Solid Blue
        for (uint32_t i = 0; i < screen_width * screen_height; i++) static_buffer[i] = 0x2C3E50;
    } else if (wallpaper_mode == 2) { // Grad Sunset
        for (uint32_t y = 0; y < screen_height; y++) {
            uint8_t r = 0xFC + (y * (0x2C - 0xFC) / screen_height);
            uint8_t g = 0x4C + (y * (0x53 - 0x4C) / screen_height);
            uint8_t b = 0x02 + (y * (0x84 - 0x02) / screen_height);
            uint32_t color = (r << 16) | (g << 8) | b;
            for (uint32_t x = 0; x < screen_width; x++) static_buffer[y * screen_width + x] = color;
        }
    } else { // Solid Gray
        for (uint32_t i = 0; i < screen_width * screen_height; i++) static_buffer[i] = 0x1A1A1B;
    }
}

void gui_prepare_frame() {
    // Copy the clean desktop from the static buffer to the working backbuffer.
    memcpy(backbuffer, static_buffer, screen_width * screen_height * 4);
}

// --- Mouse Cursor Bitmaps ---
static uint8_t cursor_bitmap[] = {
    2,2,0,0,0,0,0,0,0,0,0,0,
    2,1,2,0,0,0,0,0,0,0,0,0,
    2,1,1,2,0,0,0,0,0,0,0,0,
    2,1,1,1,2,0,0,0,0,0,0,0,
    2,1,1,1,1,2,0,0,0,0,0,0,
    2,1,1,1,1,1,2,0,0,0,0,0,
    2,1,1,1,1,1,1,2,0,0,0,0,
    2,1,1,1,1,1,1,1,2,0,0,0,
    2,1,1,1,2,2,2,2,2,2,0,0,
    2,1,1,2,0,0,0,0,0,0,0,0,
    2,1,2,0,0,0,0,0,0,0,0,0,
    2,2,0,0,0,0,0,0,0,0,0,0,
};

static uint8_t resize_cursor_bitmap[] = {
    2,2,0,0,0,0,0,0,0,0,0,0,
    2,1,2,0,0,0,0,0,0,0,0,0,
    0,2,1,2,0,0,0,0,0,0,0,0,
    0,0,2,1,2,0,0,0,0,0,0,0,
    0,0,0,2,1,2,0,0,0,0,0,0,
    0,0,0,0,2,1,2,0,0,0,0,0,
    0,0,0,0,0,2,1,2,0,0,0,0,
    0,0,0,0,0,0,2,1,2,0,0,0,
    0,0,0,0,0,0,0,2,1,2,0,0,
    0,0,0,0,0,0,0,0,2,1,2,0,
    0,0,0,0,0,0,0,0,0,2,1,2,
    0,0,0,0,0,0,0,0,0,0,2,2,
};

void draw_cursor(int x, int y) {
    uint8_t* bitmap = mouse_on_resize_handle ? resize_cursor_bitmap : cursor_bitmap;
    for(int i = 0; i < 12; i++) {
        for(int j = 0; j < 12; j++) {
            uint8_t pixel = bitmap[i * 12 + j];
            if (pixel == 1) {
                draw_pixel(x+j, y+i, mouse_left ? 0xFF0000 : 0xFFFFFF);
            }
            if (pixel == 2) draw_pixel(x+j, y+i, 0x000000);
        }
    }
}

void draw_start_menu() {
    int y = screen_height - 290;
    draw_rect(0, y, 160, 250, 0x222222);
    draw_rect(0, y, 160, 2, 0x444444);
    draw_rect(158, y, 2, 225, 0x111111);

    draw_string("Terminal", 15, y + 10, 0xFFFFFF);
    draw_string("Text Editor", 15, y + 35, 0xFFFFFF);
    draw_string("Calculator", 15, y + 60, 0xFFFFFF);
    draw_string("Task Manager", 15, y + 85, 0xFFFFFF);
    draw_string("Settings", 15, y + 110, 0xFFFFFF);
    draw_string("File Manager", 15, y + 135, 0xFFFFFF);
    draw_string("Play Doom", 15, y + 160, 0xFFFFFF);
    draw_string("Web Browser", 15, y + 185, 0xFFFFFF);
    draw_string("About ExileOS", 15, y + 210, 0xFFFFFF);
    draw_rect(5, y + 235, 150, 1, 0x555555);
}

void draw_taskbar() {
    draw_rect(0, screen_height - 40, screen_width, 40, 0x111111);
    draw_rect(0, screen_height - 40, screen_width, 2, 0x333333);
    draw_string("Start", 10, screen_height - 25, 0xFFFFFF);

    int start_x = 70;
    for (int i = 0; i < window_count; i++) {
        int b_x = start_x + (i * 130);
        uint32_t col = (i == window_count - 1 && !windows[i].minimized) ? 0x444444 : 0x222222;
        if (windows[i].minimized) col = 0x111111;
        draw_rect(b_x, screen_height - 38, 125, 38, col);

        char short_title[16];
        int len = 0;
        while(windows[i].title[len] && len < 14) {
            short_title[len] = windows[i].title[len];
            len++;
        }
        short_title[len] = '\0';
        draw_string(short_title, b_x + 5, screen_height - 25, 0xCCCCCC);
    }

    char time_str[10];
    get_time_string(time_str);
    draw_string(time_str, screen_width - 60, screen_height - 25, 0xFFFFFF);
}