#include "wm.h"
#include "gui.h"
#include "input.h"
#include "globals.h"
#include "string.h"
#include "game.h"
#include "fileman.h"
#include "editor.h"
#include "utils.h"
#include "fs.h"
#include "shell.h"
#include "browser.h"

window_t windows[MAX_WINDOWS];
int window_count = 0;

// Internal helpers for the loop
static int prev_mouse_x = 0;
static int prev_mouse_y = 0;
static uint8_t prev_mouse_left = 0;

void wm_update() {
    int mouse_dx = mouse_x - prev_mouse_x;
    int mouse_dy = mouse_y - prev_mouse_y;

    // 1. Handle Window Dragging & Resizing
    if (window_count > 0) {
        window_t* active = &windows[window_count - 1];
        
        if (mouse_left) {
            if (active->resizing) {
                active->w += mouse_dx;
                active->h += mouse_dy;
                if (active->w < 150) active->w = 150;
                if (active->h < 100) active->h = 100;
            } else if (active->dragging && !active->fullscreen) {
                active->x += mouse_dx;
                active->y += mouse_dy;
            }
        } else {
            active->dragging = 0;
            active->resizing = 0;
        }
    }

    // 2. Handle Clicks (Focus & Buttons)
    if (mouse_left && !prev_mouse_left) {
        // Taskbar / Start Menu checks first
        if (mouse_y >= (int)screen_height - 40) {
            if (mouse_x >= 0 && mouse_x <= 70) {
                start_menu_open = !start_menu_open;
            } else {
                int start_x = 70;
                for (int i = 0; i < window_count; i++) {
                    int b_x = start_x + (i * 130);
                    if (mouse_x >= b_x && mouse_x <= b_x + 125) {
                        if (windows[i].minimized) {
                            windows[i].minimized = 0;
                            open_window(windows[i].type);
                        } else if (i == window_count - 1) {
                            windows[i].minimized = 1;
                        } else {
                            open_window(windows[i].type);
                        }
                        start_menu_open = 0;
                        break;
                    }
                }
            }
        } else if (start_menu_open && mouse_x >= 0 && mouse_x <= 160 && mouse_y >= (int)screen_height - 290) {
            int y_rel = mouse_y - ((int)screen_height - 290);
            if (y_rel >= 10 && y_rel < 35) open_window(MODE_SHELL);
            else if (y_rel >= 35 && y_rel < 60) open_window(MODE_EDITOR);
            else if (y_rel >= 60 && y_rel < 85) open_window(MODE_CALC);
            else if (y_rel >= 85 && y_rel < 110) open_window(MODE_MONITOR);
            else if (y_rel >= 110 && y_rel < 135) open_window(MODE_SETTINGS);
            else if (y_rel >= 135 && y_rel < 160) open_window(MODE_FILEMAN);
            else if (y_rel >= 160 && y_rel < 185) open_window(MODE_GAME);
            else if (y_rel >= 185 && y_rel < 210) open_window(MODE_BROWSER);
            else if (y_rel >= 210 && y_rel < 235) open_window(MODE_SYSINFO);
            
            start_menu_open = 0;
        } else {
            // Window interaction (Top to Bottom)
            for (int i = window_count - 1; i >= 0; i--) {
                window_t* win = &windows[i];
                if (!win->minimized && mouse_x >= win->x && mouse_x <= win->x + win->w &&
                    mouse_y >= win->y && mouse_y <= win->y + win->h) {
                    
                    open_window(win->type); // Focus it
                    win = &windows[window_count - 1];

                    // Title Bar Logic
                    if (mouse_y <= win->y + 25) {
                        if (mouse_x >= win->x + win->w - 22) close_window(window_count - 1);
                        else if (mouse_x >= win->x + win->w - 66) win->minimized = 1;
                        else if (!win->fullscreen) win->dragging = 1;
                    } 
                    // Resize handle check
                    else if (mouse_x >= win->x + win->w - 16 && mouse_y >= win->y + win->h - 16) {
                        win->resizing = 1;
                    }
                    break;
                }
            }
            // Close start menu if we clicked the desktop or a window
            start_menu_open = 0;
        }
    }

    // 3. Game Logic (MinDoom Mouse Look)
    if (window_count > 0 && windows[window_count-1].type == MODE_GAME && mouse_dx != 0) {
        // Use your fixed-point rotation code here
        int rotCos = 65522; int rotSin = 1310;
        if (mouse_dx < 0) rotSin = -rotSin;
        
        int oldDirX = p_dir_x;
        p_dir_x = (int)(((int64_t)p_dir_x * rotCos - (int64_t)p_dir_y * rotSin) >> 16);
        p_dir_y = (int)(((int64_t)oldDirX * rotSin + (int64_t)p_dir_y * rotCos) >> 16);
        // ... same for plane_x/y
    }

    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    prev_mouse_left = mouse_left;
}

void wm_draw_all() {
    for(int i = 0; i < window_count; i++) {
        if (!windows[i].minimized) {
            draw_window(&windows[i]);
        }
    }
}

// [Include your existing open_window, close_window, draw_window here]

void open_window(term_mode_t type) {
    // If window of this type exists, bring to front (Focus)
    for (int i = 0; i < window_count; i++) {
        if (windows[i].type == type) {
            window_t temp = windows[i];
            // Shift others down
            for (int j = i; j < window_count - 1; j++) windows[j] = windows[j+1];
            windows[window_count - 1] = temp;
            return;
        }
    }

    if (window_count >= MAX_WINDOWS) return;

    window_t* win = &windows[window_count++];
    win->type = type;
    win->dragging = 0;
    win->minimized = 0;
    win->fullscreen = 0;
    win->resizing = 0;

    // Default positions/sizes based on app type
    if (type == MODE_SHELL) {
        win->x = 50; win->y = 50; win->w = 600; win->h = 400;
        strcpy(win->title, "Terminal");
    } else if (type == MODE_EDITOR) {
        win->x = 100; win->y = 100; win->w = 600; win->h = 500;
        strcpy(win->title, "Text Editor");
    } else if (type == MODE_CALC) {
        win->x = 150; win->y = 150; win->w = 300; win->h = 400;
        strcpy(win->title, "Calculator");
    } else if (type == MODE_MONITOR) {
        win->x = 200; win->y = 200; win->w = 400; win->h = 300;
        strcpy(win->title, "System Monitor");
    } else if (type == MODE_SYSINFO) {
        win->x = 250; win->y = 250; win->w = 400; win->h = 300;
        strcpy(win->title, "System Info");
    } else if (type == MODE_SETTINGS) {
        win->x = 100; win->y = 100; win->w = 400; win->h = 300;
        strcpy(win->title, "Settings");
    } else if (type == MODE_FILEMAN) {
        win->x = 120; win->y = 120; win->w = 400; win->h = 400;
        strcpy(win->title, "File Manager");
        fileman_refresh();
    } else if (type == MODE_GAME) {
        win->x = 100; win->y = 100; win->w = 640; win->h = 480;
        strcpy(win->title, "MinDoom");
    } else if (type == MODE_BROWSER) {
        win->x = 80; win->y = 80; win->w = 600; win->h = 450;
        strcpy(win->title, "Web Browser");
    }
}

void close_window(int index) {
    if (index < 0 || index >= window_count) return;
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i+1];
    }
    window_count--;
}

void draw_window_buttons(window_t* win, int x, int y, int w) {
    // Minimize Button [-]
    draw_rect(x + w - 66, y + 2, 20, 20, 0x95A5A6);
    draw_char('-', x + w - 60, y + 8, 0xFFFFFF);

    // Maximize/Restore Button [+]
    draw_rect(x + w - 44, y + 2, 20, 20, 0x95A5A6);
    draw_char(win->fullscreen ? '^' : '+', x + w - 38, y + 8, 0xFFFFFF);

    // Close Button [X]
    draw_rect(x + w - 22, y + 2, 20, 20, 0xC0392B);
    draw_char('X', x + w - 16, y + 8, 0xFFFFFF);
}

void draw_window(window_t* win) {
    int x = win->x; int y = win->y; int w = win->w; int h = win->h;

    // Modern Flat Design
    draw_rect(x + 8, y + 8, w, h, 0x000000); // Darker, tighter shadow
    draw_rect(x, y, w, h, 0x1E1E1E);         // Main Body (Dark Theme)

    if (win->type == MODE_EDITOR) {
        char editor_title[128];
        strcpy(editor_title, "Editing: ");
        strcpy(editor_title + 9, editor_filename);
        draw_rect(x, y, w, 25, 0xD35400); // Orange title bar for editor
        draw_window_buttons(win, x, y, w);
        draw_string(editor_title, x + 8, y + 8, 0xFFFFFF);

        // Draw editor content
        for (int i = 0; i < editor_num_lines; i++) {
            draw_string(editor_buffer[i], x + 5, y + 35 + (i * 10), 0xE0E0E0);
        }
        // Draw cursor as a block with inverted character color
        int cursor_screen_x = x + 5 + editor_cursor_x * 8;
        int cursor_screen_y = y + 35 + editor_cursor_y * 10;
        char char_under_cursor = editor_buffer[editor_cursor_y][editor_cursor_x];
        if (char_under_cursor == '\0') char_under_cursor = ' ';
        draw_rect(cursor_screen_x, cursor_screen_y, 8, 10, 0xCCCCCC);
        draw_char(char_under_cursor, cursor_screen_x, cursor_screen_y, 0x000000);
    }
    else if (win->type == MODE_CALC) {
        draw_rect(x, y, w, 25, 0x27AE60); // Green Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("Calculator", x + 8, y + 8, 0xFFFFFF);

        // Display box
        draw_rect(x + 50, y + 100, w - 100, 40, 0x333333);
        draw_string(calc_buffer, x + 60, y + 115, 0xFFFFFF);
        draw_string("Result:", x + 50, y + 160, 0xAAAAAA);
        draw_string(calc_result, x + 110, y + 160, 0x2ECC71);

        draw_string("Type expression (e.g. 12+5) and Enter", x + 50, y + 300, 0xCCCCCC);
    }
    else if (win->type == MODE_MONITOR) {
        draw_rect(x, y, w, 25, 0xE67E22); // Carrot Orange Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("System Monitor", x + 8, y + 8, 0xFFFFFF);

        draw_string("CPU: 1 Core (x86)", x + 20, y + 50, 0xFFFFFF);
        draw_rect(x + 150, y + 50, 200, 10, 0x333333);
        draw_rect(x + 150, y + 50, 30, 10, 0x2ECC71); // Fake 15% load

        char mem_str[32];
        int total_mem = 0;
        if (global_mbinfo) total_mem = (global_mbinfo->mem_upper / 1024) + 1; // MB

        draw_string("Memory:", x + 20, y + 80, 0xFFFFFF);
        draw_rect(x + 150, y + 80, 200, 10, 0x333333);
        // Just show a visual bar representing roughly 30MB used
        draw_rect(x + 150, y + 80, 60, 10, 0x3498DB);

        simple_itoa(total_mem, mem_str);
        draw_string(mem_str, x + 360, y + 80, 0xFFFFFF);
        draw_string("MB Total", x + 390, y + 80, 0xAAAAAA);

        draw_string("Resolution:", x + 20, y + 110, 0xFFFFFF);
        char res_w[10], res_h[10];
        simple_itoa(screen_width, res_w);
        simple_itoa(screen_height, res_h);
        draw_string(res_w, x + 150, y + 110, 0xFFFFFF);
        draw_string("x", x + 190, y + 110, 0xFFFFFF);
        draw_string(res_h, x + 205, y + 110, 0xFFFFFF);
    }
    else if (win->type == MODE_SETTINGS) {
        uint32_t t_col = (theme_mode == 1) ? 0xBDC3C7 : (theme_mode == 2 ? 0x000080 : 0x34495E);
        draw_rect(x, y, w, 25, t_col);
        draw_window_buttons(win, x, y, w);
        draw_string("Settings", x + 8, y + 8, (theme_mode == 1) ? 0x000000 : 0xFFFFFF);

        draw_string("Background:", x + 20, y + 50, 0xFFFFFF);
        draw_rect(x + 120, y + 45, 40, 20, 0x243B55); // Grad Blue
        draw_rect(x + 170, y + 45, 40, 20, 0x2C3E50); // Solid Blue
        draw_rect(x + 220, y + 45, 40, 20, 0xFC4C02); // Grad Sunset
        draw_rect(x + 270, y + 45, 40, 20, 0x1A1A1B); // Solid Gray
        draw_rect(x + 120 + (wallpaper_mode * 50), y + 67, 40, 3, 0x2ECC71); // Selection Bar

        draw_string("Theme:", x + 20, y + 90, 0xFFFFFF);
        draw_rect(x + 120, y + 85, 60, 20, 0x34495E); draw_string("Dark", x + 125, y + 90, 0xFFFFFF);
        draw_rect(x + 190, y + 85, 60, 20, 0xBDC3C7); draw_string("Light", x + 195, y + 90, 0x000000);
        draw_rect(x + 260, y + 85, 60, 20, 0x000080); draw_string("Retro", x + 265, y + 90, 0xFFFFFF);
        draw_rect(x + 120 + (theme_mode == 1 ? 70 : (theme_mode == 2 ? 140 : 0)), y + 107, 60, 3, 0x2ECC71);
    }
    else if (win->type == MODE_GAME) {
        draw_rect(x, y, w, 25, 0x800000); // DarkRed Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("MinDoom (WASD + Mouse)", x + 8, y + 8, 0xFFFFFF);

        // Raycasting Rendering Loop
        int view_w = w;
        int view_h = h - 25;
        int view_x = x;
        int view_y = y + 25;

        // Draw Ceiling and Floor
        draw_rect(view_x, view_y, view_w, view_h / 2, 0x333333);
        draw_rect(view_x, view_y + view_h / 2, view_w, view_h / 2, 0x555555);

        // Cast a ray for every vertical column
        for(int col = 0; col < view_w; col++) {
            // Calculate ray position and direction
            int cameraX = ((2 * col * F_MUL) / view_w) - F_MUL;
            int rayDirX = p_dir_x + ((p_plane_x * cameraX) >> 16);
            int rayDirY = p_dir_y + ((p_plane_y * cameraX) >> 16);

            int mapX = TO_INT(p_x);
            int mapY = TO_INT(p_y);

            // Length of ray from one x or y-side to next x or y-side
            // deltaDist = abs(1 / rayDir)
            int deltaDistX = (rayDirX == 0) ? 0x7FFFFFFF : ABS((int)(((int64_t)F_MUL * F_MUL) / rayDirX));
            int deltaDistY = (rayDirY == 0) ? 0x7FFFFFFF : ABS((int)(((int64_t)F_MUL * F_MUL) / rayDirY));

            int stepX, stepY, sideDistX, sideDistY, side;

            if (rayDirX < 0) {
                stepX = -1;
                sideDistX = ((int64_t)(p_x - TO_FIX(mapX)) * deltaDistX) >> 16;
            } else {
                stepX = 1;
                sideDistX = ((int64_t)(TO_FIX(mapX + 1) - p_x) * deltaDistX) >> 16;
            }
            if (rayDirY < 0) {
                stepY = -1;
                sideDistY = ((int64_t)(p_y - TO_FIX(mapY)) * deltaDistY) >> 16;
            } else {
                stepY = 1;
                sideDistY = ((int64_t)(TO_FIX(mapY + 1) - p_y) * deltaDistY) >> 16;
            }

            // DDA Wall Hit
            int hit = 0;
            while (hit == 0) {
                if (sideDistX < sideDistY) { sideDistX += deltaDistX; mapX += stepX; side = 0; }
                else                       { sideDistY += deltaDistY; mapY += stepY; side = 1; }
                if (world_map[mapX][mapY] > 0) hit = 1;
            }

            int perpWallDist;
            if (side == 0) perpWallDist = sideDistX - deltaDistX;
            else           perpWallDist = sideDistY - deltaDistY;
            if (perpWallDist <= 0) perpWallDist = 1;

            int lineHeight = (int)((view_h * F_MUL) / perpWallDist);
            int drawStart = -lineHeight / 2 + view_h / 2;
            if (drawStart < 0) drawStart = 0;
            int drawEnd = lineHeight / 2 + view_h / 2;
            if (drawEnd >= view_h) drawEnd = view_h - 1;

            int color = 0xAA0000; // Red
            if (world_map[mapX][mapY] == 2) color = 0x00AA00; // Green
            if (side == 1) color = (color >> 1) & 0x7F7F7F; // Shade darker for one side

            draw_rect(view_x + col, view_y + drawStart, 1, drawEnd - drawStart, color);
        }
    }
    else if (win->type == MODE_FILEMAN) {
        draw_rect(x, y, w, 25, 0xF1C40F); // Yellow Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("File Manager", x + 8, y + 8, 0x000000); // Black text on Yellow

        for (int i = 0; i < fileman_num_entries; i++) {
            // Get basename (part after the last /)
            char* basename = fileman_entries[i].name;
            for(int k=0; fileman_entries[i].name[k]; k++) {
                if (fileman_entries[i].name[k] == '/') {
                    basename = fileman_entries[i].name + k + 1;
                }
            }

            if (fileman_entries[i].flags == FS_DIRECTORY) {
                // Draw directories in Gold/Yellow
                draw_string("[]", x + 10, y + 35 + (i * 15), 0xF1C40F);
                draw_string(basename, x + 30, y + 35 + (i * 15), 0xF1C40F);
            } else {
                // Draw files in standard Grey
                draw_string(basename, x + 10, y + 35 + (i * 15), 0xE0E0E0);
            }
        }
    }
    else if (win->type == MODE_SYSINFO) {
        draw_rect(x, y, w, 25, 0x8E44AD); // Purple Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("About ExileOS", x + 8, y + 8, 0xFFFFFF);

        char ver_str[64];
        strcpy(ver_str, KERNEL_NAME);
        strcat(ver_str, " Kernel v");
        strcat(ver_str, KERNEL_VERSION);
        draw_string(ver_str, x + 20, y + 50, 0xFFFFFF);

        strcpy(ver_str, "Developed by ");
        strcat(ver_str, KERNEL_VENDOR);
        draw_string(ver_str, x + 20, y + 70, 0xAAAAAA);

        draw_string("Resolution: 1920x1080", x + 20, y + 110, 0xFFFFFF);
        if (global_mbinfo) {
             // We could format memory info here if we had printf
             draw_string("Multiboot Info Detected", x + 20, y + 130, 0x00FF00);
        }
    }
    else if (win->type == MODE_BROWSER) {
        draw_rect(x, y, w, 25, 0x2980B9); // Blue Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string("Exile Explorer", x + 8, y + 8, 0xFFFFFF);

        // URL Bar
        draw_rect(x + 5, y + 30, w - 10, 20, 0x333333);
        draw_string(browser_url, x + 10, y + 35, 0xCCCCCC);

        // Content Area
        draw_rect(x + 5, y + 55, w - 10, h - 60, 0xFFFFFF);
        
        char line_buf[76];
        int line_y = y + 65;
        int p = 0;
        for(int i = 0; browser_content[i] && line_y < y + h - 20; i++) {
            if(browser_content[i] == '\n' || p >= 74) {
                line_buf[p] = '\0';
                draw_string(line_buf, x + 12, line_y, 0x333333);
                line_y += 12;
                p = 0;
            } else if (browser_content[i] >= 32) {
                line_buf[p++] = browser_content[i];
            }
        }
    }
    else { // MODE_SHELL (Default)
        draw_rect(x, y, w, 25, 0x2980B9);        // Blue Title Bar
        draw_window_buttons(win, x, y, w);
        draw_string(win->title, x + 8, y + 8, 0xFFFFFF);

        for (int i = 0; i < MAX_TERM_LINES; i++) {
            draw_string(term_buffer[i], x + 5, y + 35 + (i * 10), 0xCCCCCC);
        }

        // Dynamic Prompt with CWD
        char prompt[140];
        strcpy(prompt, cwd);
        strcat(prompt, ">");
        draw_string(prompt, x + 5, y + h - 20, 0x2ECC71); // Green Prompt
        int prompt_width = strlen(prompt) * 8;
        draw_string(keyboard_buffer, x + 5 + prompt_width + 8, y + h - 20, 0xFFFFFF);
    }

    // Draw resize handle on non-fullscreen windows
    if (!win->fullscreen) {
        draw_rect(x + w - 10, y + h - 10, 10, 10, 0x444444);
    }
}