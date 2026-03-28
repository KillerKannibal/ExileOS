#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "multiboot.h"
#include "font.h"
#include "fs.h"
#include "string.h"
#include "gui.h"
#include "wm.h"
#include "shell.h"
#include "rtc.h"
#include "input.h"
#include "globals.h"
#include "fileman.h"
#include "game.h"
#include "editor.h"
#include "pci.h"
#include "rtl8139.h"
#include "net.h"
#include "browser.h"

// --- Toolchain Fixes ---
// Dummy for stack probing inserted by some GCC versions
void ___chkstk_ms() {}
void __chkstk_ms() {}

void kernel_main(struct multiboot_info* mbinfo) {
    init_gdt();
    // Set the kernel stack for TSS (used when switching from Ring 3 to Ring 0)
    uint32_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    tss.esp0 = esp;

    init_idt();

    // 1. Verify we actually HAVE a framebuffer before touching VGA
    if (!(mbinfo->flags & (1 << 11)) && !(mbinfo->flags & (1 << 12))) {
        // If we have no graphics, we can't draw the blue milestone. 
        // Just hang so QEMU doesn't fly into the void.
        while(1);
    }

    init_vga(mbinfo);

    uint32_t mod_start = 0;
    // Find the initrd module loaded by GRUB
    if (mbinfo->flags & (1 << 3)) { // Check bit 3 for modules
        if (mbinfo->mods_count > 0) {
            multiboot_module_t* mod = (multiboot_module_t*)(uint32_t)mbinfo->mods_addr;
            // Final safety: if mod_start is within the kernel itself, something is wrong
            if (mod->mod_start < 0x100000) {
                 init_vga(mbinfo);
                 draw_string("Error: Initrd module overlap or missing.", 10, 10, 0xFF0000);
                 while(1);
            }
            mod_start = mod->mod_start;
        }
    }

    mouse_install();
    __asm__ volatile("sti");

    // Initialize keyboard buffer
    keyboard_buffer[0] = '\0';
    kb_buffer_len = 0;

    // --- Init VFS and Read File ---
    init_fs(mod_start);

    // Scan PCI for hardware discovery
    pci_init();

    // Initialize Networking Stack
    net_init();

    // Initialize Networking
    rtl8139_init();

    // Initialize Browser State
    browser_init();

    // Pre-clear the buffers to prevent static
    draw_rect(0, 0, screen_width, screen_height, 0x000000);
    screen_update();

    init_desktop_buffer(wallpaper_mode);

    // Welcome message
    term_print("Welcome to ExileOS!");
    term_print("Type 'help' for commands. You are a God here.");

    uint8_t prev_mouse_left = 0;
    int prev_mouse_x = 0;
    int prev_mouse_y = 0;
    open_window(MODE_SHELL); // Open initial terminal

    while(1) {
        int mouse_dx = mouse_x - prev_mouse_x;
        int mouse_dy = mouse_y - prev_mouse_y;

        // --- Check for Resize Handle Hover ---
        mouse_on_resize_handle = 0;
        // Only check when not actively dragging/resizing to prevent cursor flicker
        if (!mouse_left) {
            for (int i = window_count - 1; i >= 0; i--) {
                window_t* win = &windows[i];
                if (!win->minimized && !win->fullscreen) {
                    int resize_x = win->x + win->w;
                    int resize_y = win->y + win->h;
                    if (mouse_x >= resize_x - 16 && mouse_x <= resize_x &&
                        mouse_y >= resize_y - 16 && mouse_y <= resize_y) {
                        mouse_on_resize_handle = 1;
                        break; // Found top-most, so stop
                    }
                }
            }
        }

        // --- Click Logic ---
        if (mouse_left && !prev_mouse_left) { // On Click Press
            if (mouse_on_resize_handle) {
                // Find which window to resize (topmost one under cursor)
                for (int i = window_count - 1; i >= 0; i--) {
                    window_t* win = &windows[i];
                     if (!win->minimized && !win->fullscreen && mouse_x >= win->x + win->w - 16 && mouse_y >= win->y + win->h - 16) {
                        open_window(win->type); // Bring to front
                        windows[window_count - 1].resizing = 1;
                        break;
                    }
                }
            } else if (mouse_x >= 0 && mouse_x <= 70 && mouse_y >= (int)screen_height - 40) {
                start_menu_open = !start_menu_open;
            } else if (start_menu_open && mouse_x >= 0 && mouse_x <= 160 && mouse_y >= (int)screen_height - 290) {
                int y_rel = mouse_y - ((int)screen_height - 290);

                if (y_rel >= 10 && y_rel < 35) { // Terminal
                    open_window(MODE_SHELL);
                    start_menu_open = 0;
                } else if (y_rel >= 35 && y_rel < 60) { // Editor
                    start_editor("new.txt");
                    start_menu_open = 0;
                } else if (y_rel >= 60 && y_rel < 85) { // Calculator
                    open_window(MODE_CALC);
                    calc_len = 0; calc_buffer[0] = '\0'; strcpy(calc_result, "0");
                    start_menu_open = 0;
                } else if (y_rel >= 85 && y_rel < 110) { // Resource Monitor
                    open_window(MODE_MONITOR);
                    start_menu_open = 0;
                } else if (y_rel >= 110 && y_rel < 135) { // Settings
                    open_window(MODE_SETTINGS);
                    start_menu_open = 0;
                } else if (y_rel >= 135 && y_rel < 160) { // File Manager
                    open_window(MODE_FILEMAN);
                    start_menu_open = 0;
                } else if (y_rel >= 160 && y_rel < 185) { // Game
                    open_window(MODE_GAME);
                    start_menu_open = 0;
                } else if (y_rel >= 185 && y_rel < 210) { // Browser
                    open_window(MODE_BROWSER);
                    start_menu_open = 0;
                } else if (y_rel >= 210 && y_rel < 235) { // About
                    open_window(MODE_SYSINFO);
                    start_menu_open = 0;
                }
            } else {
                // Check Taskbar App List Click
                if (mouse_y >= (int)screen_height - 40 && mouse_x > 70) {
                    int clicked_idx = (mouse_x - 70) / 130;
                    if (clicked_idx >= 0 && clicked_idx < window_count) {
                        // Logic:
                        // 1. If minimized, restore and focus.
                        // 2. If not active, focus.
                        // 3. If active and top, minimize.

                        // We need to match the specific window in the array.
                        // Since draw_taskbar iterates 0..window_count, clicked_idx corresponds to index.
                        if (windows[clicked_idx].minimized) {
                            windows[clicked_idx].minimized = 0;
                            open_window(windows[clicked_idx].type); // Bring to front
                        } else if (clicked_idx == window_count - 1) {
                            // Already active, so minimize
                            windows[clicked_idx].minimized = 1;
                        } else {
                            open_window(windows[clicked_idx].type); // Bring to front
                        }
                    }
                }

                if (start_menu_open) start_menu_open = 0; // Clicked outside
                // Window Management (Reverse order to click top-most first)
                for(int i = window_count - 1; i >= 0; i--) {
                    window_t* win = &windows[i];
                    if (!win->minimized &&
                        mouse_x >= win->x && mouse_x <= win->x + win->w &&
                        mouse_y >= win->y && mouse_y <= win->y + win->h) {

                        // Bring to front
                        open_window(win->type); // Re-opening essentially focuses it in our logic
                        win = &windows[window_count - 1]; // Update pointer to new location

                        // --- Window Controls (Title Bar) ---
                        if (mouse_y <= win->y + 25) {
                             // Close [X] (Rightmost)
                             if (mouse_x >= win->x + win->w - 22) {
                            close_window(window_count - 1);
                        }
                             // Maximize [^] (Middle)
                             else if (mouse_x >= win->x + win->w - 44) {
                                 if (!win->fullscreen) {
                                     // Save old state
                                     win->old_x = win->x; win->old_y = win->y;
                                     win->old_w = win->w; win->old_h = win->h;
                                     // Set fullscreen (minus taskbar)
                                     win->x = 0; win->y = 0;
                                     win->w = screen_width; win->h = screen_height - 40;
                                     win->fullscreen = 1;
                                     win->dragging = 0;
                                 } else {
                                     // Restore
                                     win->x = win->old_x; win->y = win->old_y;
                                     win->w = win->old_w; win->h = win->old_h;
                                     win->fullscreen = 0;
                                 }
                             }
                             // Minimize [-] (Leftmost)
                             else if (mouse_x >= win->x + win->w - 66) {
                                 win->minimized = 1;
                             }
                             // Title Bar Dragging (if not buttons and not fullscreen)
                             else if (!win->fullscreen) {
                            win->dragging = 1;
                        }
                        }

                        // Check Settings Buttons
                        else if (win->type == MODE_SETTINGS) {
                            // Wallpaper Buttons
                            for (int i = 0; i < 4; i++) {
                                int bx = win->x + 120 + (i * 50);
                                if (mouse_x >= bx && mouse_x <= bx + 40 &&
                                    mouse_y >= win->y + 45 && mouse_y <= win->y + 65) {
                                    wallpaper_mode = i;
                                    init_desktop_buffer(wallpaper_mode);
                                }
                            }

                            // Theme Buttons
                            if (mouse_y >= win->y + 85 && mouse_y <= win->y + 105) {
                                if (mouse_x >= win->x + 120 && mouse_x <= win->x + 180) theme_mode = 0;
                                else if (mouse_x >= win->x + 190 && mouse_x <= win->x + 250) theme_mode = 1;
                                else if (mouse_x >= win->x + 260 && mouse_x <= win->x + 320) theme_mode = 2;
                            }
                        }
                        // Check File Manager clicks
                        else if (win->type == MODE_FILEMAN) {
                            if (mouse_y > win->y + 25) { // Below title bar
                                int item_index = (mouse_y - (win->y + 35)) / 15;
                                if (item_index >= 0 && item_index < fileman_num_entries) {
                                    dirent_t* item = &fileman_entries[item_index];

                                    if (item->flags == FS_DIRECTORY) {
                                        // Handle Directory Navigation
                                        if (strcmp(item->name, "..") == 0) {
                                            // Go Up
                                            int len = strlen(cwd);
                                            while (len > 1) { // Don't strip root slash
                                                len--;
                                                if (cwd[len] == '/') {
                                                    cwd[len] = '\0';
                                                    if (len == 0) strcpy(cwd, "/");
                                                    break;
                                                }
                                            }
                                        } else {
                                            // Go Down
                                            strcpy(cwd, "/");
                                            strcat(cwd, item->name);
                                        }
                                        fileman_refresh();
                                    } else {
                                        start_editor(item->name);
                                    }
                                }
                            }
                        }
                        break; // Stop after hitting top window
                    }
                }
            }
        }

        // Handle Dragging
        if (mouse_left && window_count > 0) {
            window_t* active = &windows[window_count - 1];
            if (active->resizing) {
                active->w += mouse_dx;
                active->h += mouse_dy;

                // Clamp to minimum size
                if (active->w < 150) active->w = 150;
                if (active->h < 100) active->h = 100;
            } else if (active->dragging && !active->fullscreen) {
                active->x += mouse_dx;
                active->y += mouse_dy;
            }
        } else if (!mouse_left && window_count > 0) {
            windows[window_count - 1].dragging = 0;
            windows[window_count - 1].resizing = 0;
        }

        // Handle mouse-look for the game
        if (window_count > 0) {
            window_t* active = &windows[window_count - 1];
            if (active->type == MODE_GAME && mouse_dx != 0) {
                // Apply a small, fixed rotation for each unit of mouse_dx.
                // This is proportional but avoids complex sin/cos calculations.

                // Use a smaller angle for mouse than keyboard for smoothness.
                // rot_angle = 0.02 radians. cos(0.02) ~ 0.9998, sin(0.02) ~ 0.02
                int rotCos = 65522; // cos(0.02) * 65536
                int rotSin = 1310;  // sin(0.02) * 65536

                // Determine direction and number of rotation steps
                int steps = mouse_dx;
                if (steps < 0) {
                    steps = -steps;
                    rotSin = -rotSin; // Negating sin rotates in the opposite direction
                }

                // To prevent stalling on huge mouse movements, cap the steps per frame.
                if (steps > 20) steps = 20;

                for (int i = 0; i < steps; i++) {
                    int oldDirX = p_dir_x;
                    p_dir_x = (int)(((int64_t)p_dir_x * rotCos - (int64_t)p_dir_y * rotSin) >> 16);
                    p_dir_y = (int)(((int64_t)oldDirX * rotSin + (int64_t)p_dir_y * rotCos) >> 16);
                    int oldPlaneX = p_plane_x;
                    p_plane_x = (int)(((int64_t)p_plane_x * rotCos - (int64_t)p_plane_y * rotSin) >> 16);
                    p_plane_y = (int)(((int64_t)oldPlaneX * rotSin + (int64_t)p_plane_y * rotCos) >> 16);
                }
            }
        }

        prev_mouse_left = mouse_left;
        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;

        // 1. Prepare frame by copying desktop from static buffer
        gui_prepare_frame();
        draw_taskbar();

        // 2. Draw UI Elements
        for(int i = 0; i < window_count; i++) {
            if (!windows[i].minimized) {
            draw_window(&windows[i]);
            }
        }
        if (start_menu_open) draw_start_menu();

        // 3. Draw Mouse Cursor (last, so it stays on top)
        draw_cursor(mouse_x, mouse_y);

        // 4. Render the frame
        screen_update();

    }
}