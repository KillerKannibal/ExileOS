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

volatile uint8_t mouse_left = 0;

// --- Terminal / Shell Logic ---

typedef enum {
    MODE_SHELL,
    MODE_EDITOR,
    MODE_CALC,
    MODE_SYSINFO,
    MODE_MONITOR,
    MODE_SETTINGS,
    MODE_FILEMAN
} term_mode_t;

// --- Window Manager State ---
typedef struct {
    int x, y, w, h;
    term_mode_t type;
    char title[64];
    int dragging;
} window_t;

#define MAX_WINDOWS 10
window_t windows[MAX_WINDOWS];
int window_count = 0;

// Global Multiboot Info Pointer
struct multiboot_info* global_mbinfo = NULL;

// --- Start Menu State ---
int start_menu_open = 0;
int wallpaper_mode = 0; // 0 = Gradient, 1 = Solid
char cwd[128] = "/";    // Current Working Directory

#define MAX_TERM_LINES 20
#define MAX_TERM_COLS  70

char term_buffer[MAX_TERM_LINES][MAX_TERM_COLS];

// A simple buffer for typed characters (for shell).
#define KB_BUFFER_SIZE 256
char keyboard_buffer[KB_BUFFER_SIZE];
int kb_buffer_len = 0;

// --- Editor State ---
#define EDITOR_MAX_LINES 100
#define EDITOR_MAX_COLS  70 // Must be <= MAX_TERM_COLS
char editor_buffer[EDITOR_MAX_LINES][EDITOR_MAX_COLS];
int editor_num_lines = 0;
int editor_cursor_x = 0;
int editor_cursor_y = 0;
char editor_filename[128];

// --- Calculator State ---
char calc_buffer[64];
int calc_len = 0;
char calc_result[64] = "0";

// --- RTC (Real Time Clock) ---
#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

uint8_t get_rtc_register(int reg) {
    outb(CMOS_ADDRESS, reg);
    return inb(CMOS_DATA);
}

void get_time_string(char* buf) {
    // Wait for "update in progress" flag to clear to avoid reading garbage
    while (get_rtc_register(0x0A) & 0x80);

    uint8_t second = get_rtc_register(0x00);
    uint8_t minute = get_rtc_register(0x02);
    uint8_t hour   = get_rtc_register(0x04);
    uint8_t registerB = get_rtc_register(0x0B);

    // Convert BCD to binary if necessary
    if (!(registerB & 0x04)) {
        second = (second & 0x0F) + ((second / 16) * 10);
        minute = (minute & 0x0F) + ((minute / 16) * 10);
        hour   = ((hour & 0x0F) + (((hour & 0x70) / 16) * 10)) | (hour & 0x80);
    }

    // Simple formatting HH:MM
    buf[0] = (hour / 10) + '0';
    buf[1] = (hour % 10) + '0';
    buf[2] = ':';
    buf[3] = (minute / 10) + '0';
    buf[4] = (minute % 10) + '0';
    buf[5] = '\0';
}

// --- File Manager State ---
#define FILEMAN_MAX_ENTRIES 50
dirent_t fileman_entries[FILEMAN_MAX_ENTRIES];
int fileman_num_entries = 0;

// --- Helpers ---

int simple_atoi(const char* s) {
    int res = 0;
    int sign = 1;
    if (*s == '-') { sign = -1; s++; }
    while (*s >= '0' && *s <= '9') {
        res = res * 10 + (*s - '0');
        s++;
    }
    return res * sign;
}

void simple_itoa(int val, char* buf) {
    if (val == 0) {
        buf[0] = '0'; buf[1] = '\0'; return;
    }
    int i = 0;
    int is_neg = 0;
    if (val < 0) {
        is_neg = 1;
        val = -val;
    }
    while (val != 0) {
        buf[i++] = (val % 10) + '0';
        val /= 10;
    }
    if (is_neg) buf[i++] = '-';
    buf[i] = '\0';
    // Reverse string
    for(int j=0; j<i/2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i-j-1];
        buf[i-j-1] = tmp;
    }
}

void term_scroll() {
    // Move every line up by one
    for (int i = 0; i < MAX_TERM_LINES - 1; i++) {
        strcpy(term_buffer[i], term_buffer[i+1]);
    }
    // Clear the last line
    term_buffer[MAX_TERM_LINES - 1][0] = '\0';
}

void term_print(const char* msg) {
    term_scroll();
    // Copy message to the last line (safe copy)
    char* dest = term_buffer[MAX_TERM_LINES - 1];
    int i = 0;
    while (msg[i] && i < MAX_TERM_COLS - 1) {
        dest[i] = msg[i];
        i++;
    }
    dest[i] = '\0';
}

void fileman_refresh() {
    fileman_num_entries = 0;

    // Add ".." entry if we are not at root
    if (strcmp(cwd, "/") != 0) {
        strcpy(fileman_entries[0].name, "..");
        fileman_entries[0].ino = 0;
        fileman_entries[0].flags = FS_DIRECTORY;
        fileman_num_entries++;
    }

    int i = 0;
    dirent_t* de;
    while((de = readdir_fs(fs_root, i)) != 0 && fileman_num_entries < FILEMAN_MAX_ENTRIES) {
        // Filter entries based on CWD
        int show = 0;
        
        if (strcmp(cwd, "/") == 0) {
            // Root: Show files that do not contain '/'
            int has_slash = 0;
            for(int k=0; de->name[k]; k++) if(de->name[k] == '/') has_slash = 1;
            if (!has_slash) show = 1;
        } else {
            // Subdir: Show files that start with "cwd/"
            // cwd is like "/folder", de->name is "folder/file.txt"
            char prefix[128];
            strcpy(prefix, cwd + 1); // Skip leading '/'
            strcat(prefix, "/");
            
            if (strncmp(de->name, prefix, strlen(prefix)) == 0) {
                // And ensure it's a direct child (no extra slashes)
                char* remainder = de->name + strlen(prefix);
                int has_slash = 0;
                for(int k=0; remainder[k]; k++) if(remainder[k] == '/') has_slash = 1;
                if (!has_slash) show = 1;
            }
        }

        if (show) {
            strcpy(fileman_entries[fileman_num_entries].name, de->name);
            fileman_entries[fileman_num_entries].ino = de->ino;
            fileman_entries[fileman_num_entries].flags = de->flags;
            fileman_num_entries++;
        }
        i++;
    }
}

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
    }
}

void close_window(int index) {
    if (index < 0 || index >= window_count) return;
    for (int i = index; i < window_count - 1; i++) {
        windows[i] = windows[i+1];
    }
    window_count--;
}

void start_editor(const char* filename) {
    // Clear editor state
    for(int i=0; i<EDITOR_MAX_LINES; i++) editor_buffer[i][0] = '\0';
    editor_num_lines = 0;
    editor_cursor_x = 0;
    editor_cursor_y = 0;
    strcpy(editor_filename, filename);

    fs_node_t* node = finddir_fs(fs_root, (char*)filename);
    if (node) {
        // Read file line by line
        char file_buf[8192]; // Max file size to read
        uint32_t size = read_fs(node, 0, sizeof(file_buf)-1, (uint8_t*)file_buf);
        file_buf[size] = '\0';

        int current_line = 0;
        int current_col = 0;
        for(uint32_t i=0; i<size && current_line < EDITOR_MAX_LINES; i++) {
            if (file_buf[i] == '\n') {
                editor_buffer[current_line][current_col] = '\0';
                current_line++;
                current_col = 0;
            } else if (file_buf[i] != '\r' && current_col < EDITOR_MAX_COLS - 1) {
                editor_buffer[current_line][current_col++] = file_buf[i];
            }
        }
        editor_buffer[current_line][current_col] = '\0';
        editor_num_lines = current_line + 1;
        if (editor_num_lines == 0) editor_num_lines = 1;
    } else {
        // New file
        editor_num_lines = 1;
    }
    
    open_window(MODE_EDITOR);
}

void exec_command(char* input) {
    if (input[0] == '\0') return;

    // Echo command to terminal
    char echo[MAX_TERM_COLS];
    echo[0] = '>'; echo[1] = ' ';
    strcpy(echo + 2, input);
    term_print(echo);

    if (strcmp(input, "help") == 0) {
        term_print("Available commands:");
        term_print("  help  - Show this message");
        term_print("  clear - Clear the terminal");
        term_print("  cat <f> - Show file content");
        term_print("  edit <f> - Edit a file");
        term_print("  echo <msg> - Print a message");
        term_print("  ver - Show kernel version");
        term_print("  cd <dir> - Change directory");
        term_print("  ls - List files");
        term_print("  mkdir <dir> - Create directory");
        term_print("  reboot - Reboot the system");
    } 
    else if (strcmp(input, "clear") == 0) {
        for(int i=0; i<MAX_TERM_LINES; i++) term_buffer[i][0] = '\0';
    }
    else if (strncmp(input, "cat ", 4) == 0) {
        char* filename = input + 4;
        fs_node_t* node = finddir_fs(fs_root, filename);
        if (node) {
            // Read whole file (up to a limit) and print line-by-line
            char file_buf[4096];
            uint32_t size = read_fs(node, 0, sizeof(file_buf)-1, (uint8_t*)file_buf);
            file_buf[size] = '\0';

            char line_buf[MAX_TERM_COLS];
            int line_idx = 0;
            for(uint32_t i=0; i<size; i++) {
                if (file_buf[i] == '\n' || line_idx >= MAX_TERM_COLS - 1) {
                    line_buf[line_idx] = '\0';
                    term_print(line_buf);
                    line_idx = 0;
                } else if (file_buf[i] != '\r') {
                    line_buf[line_idx++] = file_buf[i];
                }
            }
            if (line_idx > 0) {
                line_buf[line_idx] = '\0';
                term_print(line_buf);
            }
        } else {
            term_print("File not found.");
        }
    }
    else if (strncmp(input, "edit ", 5) == 0) {
        char* filename = input + 5;
        start_editor(filename);
        // Clear terminal for editor view
        for(int i=0; i<MAX_TERM_LINES; i++) term_buffer[i][0] = '\0';
    }
    else if (strncmp(input, "echo ", 5) == 0) {
        term_print(input + 5);
    }
    else if (strcmp(input, "ver") == 0) {
        term_print("MinOS Kernel v0.1");
    }
    else if (strcmp(input, "reboot") == 0) {
        term_print("Rebooting system...");
        // Use the 8042 keyboard controller to pulse the reset line.
        uint8_t good = 0x02;
        while (good & 0x02)
            good = inb(0x64);
        outb(0x64, 0xFE);
        while(1) asm volatile("hlt"); // Loop if reboot fails
    }
    else if (strncmp(input, "mkdir ", 6) == 0) {
        char* dirname = input + 6;
        if (mkdir_fs(fs_root, dirname)) {
            term_print("Directory created.");
            fileman_refresh(); // Refresh file manager if open
        } else {
            term_print("Error creating directory.");
        }
    }
    else if (strcmp(input, "ls") == 0) {
        int i = 0;
        dirent_t* de;
        term_print("Directory listing:");
        while((de = readdir_fs(fs_root, i)) != 0) {
            // Simple list of all files for now
            term_print(de->name);
            i++;
        }
    }
    else if (strncmp(input, "cd ", 3) == 0) {
        char* new_dir = input + 3;
        if (strcmp(new_dir, "/") == 0) {
            strcpy(cwd, "/");
        } 
        else if (strcmp(new_dir, "..") == 0) {
            // Go up one level (strip last component)
            int len = strlen(cwd);
            if (len > 1) {
                for(int i = len - 1; i >= 0; i--) {
                    if (cwd[i] == '/') {
                        cwd[i] = '\0';
                        if (i == 0) strcpy(cwd, "/"); // Back to root
                        break;
                    }
                }
            }
        } 
        else {
            // Append directory
            if (strlen(cwd) > 1) strcat(cwd, "/");
            strcat(cwd, new_dir);
        }
    }
    else {
        term_print("Unknown command.");
    }
}

void draw_taskbar() {
    draw_rect(0, screen_height - 40, screen_width, 40, 0x111111);
    draw_rect(0, screen_height - 40, screen_width, 2, 0x333333); // Highlight
    draw_string("Start", 10, screen_height - 25, 0xFFFFFF);

    // Draw Clock
    char time_str[10];
    get_time_string(time_str);
    draw_string(time_str, screen_width - 60, screen_height - 25, 0xFFFFFF);
}

void draw_start_menu() {
    int y = screen_height - 240; 
    draw_rect(0, y, 160, 200, 0x222222); // Menu Body
    draw_rect(0, y, 160, 2, 0x444444);   // Top Highlight
    draw_rect(158, y, 2, 200, 0x111111); // Side Shadow

    draw_string("Terminal", 15, y + 10, 0xFFFFFF);
    draw_string("Text Editor", 15, y + 35, 0xFFFFFF);
    draw_string("Calculator", 15, y + 60, 0xFFFFFF);
    draw_string("Resource Mon", 15, y + 85, 0xFFFFFF);
    draw_string("Settings", 15, y + 110, 0xFFFFFF);
    draw_string("File Manager", 15, y + 135, 0xFFFFFF);
    draw_string("About MinOS", 15, y + 160, 0xFFFFFF);
    
    draw_rect(5, y + 185, 150, 1, 0x555555); // Separator
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
        draw_string(editor_title, x + 8, y + 8, 0xFFFFFF);
        draw_string("ESC to Close", x + w - 90, y + 8, 0xFFFFFF);

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
        draw_string("Calculator", x + 8, y + 8, 0xFFFFFF);
        
        // Display box
        draw_rect(x + 50, y + 100, w - 100, 40, 0x333333);
        draw_string(calc_buffer, x + 60, y + 115, 0xFFFFFF);
        draw_string("Result:", x + 50, y + 160, 0xAAAAAA);
        draw_string(calc_result, x + 110, y + 160, 0x2ECC71);
        
        draw_string("Type expression (e.g. 12+5) and Enter", x + 50, y + 300, 0x777777);
    }
    else if (win->type == MODE_MONITOR) {
        draw_rect(x, y, w, 25, 0xE67E22); // Carrot Orange Title Bar
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
        draw_rect(x, y, w, 25, 0x34495E); // Dark Slate Title Bar
        draw_string("Settings", x + 8, y + 8, 0xFFFFFF);

        draw_string("Background:", x + 20, y + 50, 0xFFFFFF);
        draw_rect(x + 120, y + 45, 100, 20, 0x555555); // Fake Button
        if (wallpaper_mode == 0) draw_string("Gradient", x + 130, y + 50, 0xFFFFFF);
        else draw_string("Solid", x + 130, y + 50, 0xFFFFFF);

        draw_string("Theme:", x + 20, y + 80, 0xFFFFFF);
        draw_rect(x + 120, y + 75, 100, 20, 0x555555); // Fake Button
        draw_string("Dark", x + 130, y + 80, 0xFFFFFF);
    }
    else if (win->type == MODE_FILEMAN) {
        draw_rect(x, y, w, 25, 0xF1C40F); // Yellow Title Bar
        draw_string("File Manager", x + 8, y + 8, 0xFFFFFF);

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
        draw_string("About MinOS", x + 8, y + 8, 0xFFFFFF);

        draw_string("MinOS Kernel v0.1", x + 20, y + 50, 0xFFFFFF);
        draw_string("Developed by KillerKannibal", x + 20, y + 70, 0xAAAAAA);
        
        draw_string("Resolution: 1024x768", x + 20, y + 110, 0xFFFFFF);
        if (global_mbinfo) {
             // We could format memory info here if we had printf
             draw_string("Multiboot Info Detected", x + 20, y + 130, 0x00FF00);
        }
    }
    else { // MODE_SHELL (Default)
        draw_rect(x, y, w, 25, 0x2980B9);        // Blue Title Bar
        draw_string(win->title, x + 8, y + 8, 0xFFFFFF);
        
        // Close Button
        draw_rect(x + w - 22, y + 2, 20, 20, 0xC0392B);
        draw_char('X', x + w - 15, y + 8, 0xFFFFFF);

        for (int i = 0; i < MAX_TERM_LINES; i++) {
            draw_string(term_buffer[i], x + 5, y + 35 + (i * 10), 0xCCCCCC);
        }
        
        // Dynamic Prompt with CWD
        char prompt[140];
        strcpy(prompt, cwd);
        strcat(prompt, ">");
        draw_string(prompt, x + 5, y + h - 20, 0x2ECC71); // Green Prompt
        int prompt_width = strlen(prompt) * 8;
        draw_string(keyboard_buffer, x + 5 + prompt_width + 8, y + h - 20, 0x000000);
    }
}

// --- Mouse Cursor ---

// 12x12 bitmap. 1=White, 2=Black/Border, 0=Transparent
uint8_t cursor_bitmap[] = {
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

void draw_cursor(int x, int y) {
    for(int i = 0; i < 12; i++) {
        for(int j = 0; j < 12; j++) {
            uint8_t pixel = cursor_bitmap[i * 12 + j];
            if (pixel == 1) {
                // Turn Red (0xFF0000) if holding left click, otherwise White
                draw_pixel(x+j, y+i, mouse_left ? 0xFF0000 : 0xFFFFFF);
            }
            if (pixel == 2) draw_pixel(x+j, y+i, 0x000000);
        }
    }
}

// --- Input Globals ---
volatile int mouse_x = 512;
volatile int mouse_y = 384;
volatile uint8_t mouse_cycle = 0;
uint8_t mouse_byte[3];

// --- Keyboard Logic ---

// A basic US QWERTY scancode to ASCII map.
unsigned char keyboard_map[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

// --- Mouse Logic ---

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) { if ((inb(0x64) & 1) == 1) return; }
    } else {
        while (timeout--) { if ((inb(0x64) & 2) == 0) return; }
    }
}

void mouse_write(uint8_t write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, write);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_install() {
    uint8_t status;
    mouse_wait(1);
    outb(0x64, 0xA8);
    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = (inb(0x60) | 2);
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();
}

void mouse_handler() {
    // When IRQ12 fires, we MUST read the data port to clear the interrupt.
    uint8_t data = inb(0x60);

    // This is a state machine for handling 3-byte mouse packets.
    // The first byte of a packet always has bit 3 set. We can use this
    // to synchronize if we ever get out of step.
    if (mouse_cycle == 0) {
        // Expecting the first byte (status byte). Check for the sync bit (bit 3).
        if ((data & 0x08) != 0) {
            mouse_byte[0] = data;
            mouse_cycle++;
        }
        // If sync bit is not set, we are out of sync. Discard this byte
        // and wait for a byte that looks like a proper status byte.
    } else {
        // Expecting the second or third byte (movement data)
        mouse_byte[mouse_cycle++] = data;

        if (mouse_cycle >= 3) {
            mouse_cycle = 0; // Reset for the next packet

            // We have a full packet, now interpret it.
            int x_move = mouse_byte[1];
            int y_move = mouse_byte[2];

            // Handle sign extension for 9-bit signed values
            if (mouse_byte[0] & 0x10) x_move |= 0xFFFFFF00; // X sign bit
            if (mouse_byte[0] & 0x20) y_move |= 0xFFFFFF00; // Y sign bit
            
            // Update button state (Bit 0 = Left Click)
            mouse_left = (mouse_byte[0] & 0x01);

            // Y-movement is inverted in screen coordinates
            mouse_x += x_move;
            mouse_y -= y_move;

            // Clamp cursor to screen bounds
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)screen_width) mouse_x = screen_width - 1;
            if (mouse_y >= (int)screen_height) mouse_y = screen_height - 1;
        }
    }
}

void keyboard_handler() {
    uint8_t scancode = inb(0x60);
    
    if (scancode & 0x80) return; // Ignore key releases

    if (window_count == 0) return; // No windows open
    
    window_t* active_win = &windows[window_count - 1]; // Top-most window

    if (active_win->type == MODE_EDITOR) {
        if (scancode == 1) { // ESC
            close_window(window_count - 1);
            return;
        }

        char c = keyboard_map[scancode];
        char* line = editor_buffer[editor_cursor_y];
        int line_len = strlen(line);

        if (c == '\b') { // Backspace
            if (editor_cursor_x > 0) {
                memmove(&line[editor_cursor_x - 1], &line[editor_cursor_x], line_len - editor_cursor_x + 1);
                editor_cursor_x--;
            } else if (editor_cursor_y > 0) {
                int prev_line_len = strlen(editor_buffer[editor_cursor_y - 1]);
                if (prev_line_len + line_len < EDITOR_MAX_COLS) {
                    strcpy(&editor_buffer[editor_cursor_y - 1][prev_line_len], line);
                    for (int i = editor_cursor_y; i < editor_num_lines - 1; i++) {
                        strcpy(editor_buffer[i], editor_buffer[i+1]);
                    }
                    editor_num_lines--;
                    editor_cursor_y--;
                    editor_cursor_x = prev_line_len;
                }
            }
        } else if (c == '\n') { // Enter
            if (editor_num_lines < EDITOR_MAX_LINES - 1) {
                for (int i = editor_num_lines; i > editor_cursor_y + 1; i--) {
                    strcpy(editor_buffer[i], editor_buffer[i-1]);
                }
                strcpy(editor_buffer[editor_cursor_y + 1], &line[editor_cursor_x]);
                line[editor_cursor_x] = '\0';
                editor_num_lines++;
                editor_cursor_y++;
                editor_cursor_x = 0;
            }
        } else if (c != 0) { // Printable character
            if (line_len < EDITOR_MAX_COLS - 1) {
                memmove(&line[editor_cursor_x + 1], &line[editor_cursor_x], line_len - editor_cursor_x + 1);
                line[editor_cursor_x] = c;
                editor_cursor_x++;
            }
        }
        // Clamp cursor X
        line_len = strlen(editor_buffer[editor_cursor_y]);
        if (editor_cursor_x > line_len) editor_cursor_x = line_len;
    } 
    else if (active_win->type == MODE_CALC) {
        char c = keyboard_map[scancode];
        if (c == '\b') {
            if (calc_len > 0) calc_buffer[--calc_len] = '\0';
        } 
        else if (c == '\n') {
             // Parse A op B
             int op_idx = -1;
             char op = 0;
             for(int i=0; i<calc_len; i++) {
                 if (calc_buffer[i] == '+' || calc_buffer[i] == '-' || 
                     calc_buffer[i] == '*' || calc_buffer[i] == '/') {
                     op_idx = i;
                     op = calc_buffer[i];
                     break;
                 }
             }
             if (op_idx != -1) {
                 char left[32], right[32];
                 strcpy(left, calc_buffer); left[op_idx] = '\0';
                 strcpy(right, calc_buffer + op_idx + 1);
                 int a = simple_atoi(left);
                 int b = simple_atoi(right);
                 int res = 0;
                 if (op == '+') res = a + b;
                 if (op == '-') res = a - b;
                 if (op == '*') res = a * b;
                 if (op == '/' && b != 0) res = a / b;
                 simple_itoa(res, calc_result);
             }
        } 
        else if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '*' || c == '/') {
            if (calc_len < 30) { calc_buffer[calc_len++] = c; calc_buffer[calc_len] = '\0'; }
        }
    }
    else if (active_win->type == MODE_SHELL) { // MODE_SHELL
        char c = keyboard_map[scancode];
        if (c == '\b') {
            if (kb_buffer_len > 0) kb_buffer_len--;
        } else if (c == '\n') { // Handle Enter
            keyboard_buffer[kb_buffer_len] = '\0';
            exec_command(keyboard_buffer);
            kb_buffer_len = 0;
        } else if (c != 0 && kb_buffer_len < KB_BUFFER_SIZE - 1) {
            keyboard_buffer[kb_buffer_len++] = c;
        }
        keyboard_buffer[kb_buffer_len] = '\0';
    }
}

// --- Kernel Entry ---

void kernel_main(struct multiboot_info* mbinfo) {
    global_mbinfo = mbinfo; // Save for sysinfo app

    init_gdt();
    init_idt();

    // Check for Framebuffer Info (Bit 12), not just VBE Info (Bit 11)
    if (!(mbinfo->flags & (1 << 12))) {
        while(1); 
    }

    init_vga(mbinfo);
    uintptr_t mod_start = 0;
    // Find the initrd module loaded by GRUB
    if (mbinfo->flags & (1 << 3)) { // Check bit 3 for modules
        if (mbinfo->mods_count > 0) {
            multiboot_module_t* mod = (multiboot_module_t*)(uintptr_t)mbinfo->mods_addr;
            mod_start = mod->mod_start;
        }
    }


    mouse_install();
    asm volatile("sti");

    // Initialize keyboard buffer
    keyboard_buffer[0] = '\0';
    kb_buffer_len = 0;

    // --- Init VFS and Read File ---
    init_fs(mod_start);
    
    // Welcome message
    term_print("Welcome to MinOS!");
    term_print("Type 'help' for commands.");

    uint8_t prev_mouse_left = 0;
    int prev_mouse_x = 0;
    int prev_mouse_y = 0;
    open_window(MODE_SHELL); // Open initial terminal

    while(1) {
        int mouse_dx = mouse_x - prev_mouse_x;
        int mouse_dy = mouse_y - prev_mouse_y;

        // --- Click Logic ---
        if (mouse_left && !prev_mouse_left) { // On Click Press
            // Check Start Button Click (0, H-40, 60, 40)
            if (mouse_x >= 0 && mouse_x <= 60 && mouse_y >= (int)screen_height - 40) {
                start_menu_open = !start_menu_open;
            }
            // Check Start Menu Items
            else if (start_menu_open && mouse_x >= 0 && mouse_x <= 160 && mouse_y >= (int)screen_height - 240) {
                int y_rel = mouse_y - ((int)screen_height - 240);
                
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
                } else if (y_rel >= 160 && y_rel < 185) { // About
                    open_window(MODE_SYSINFO);
                    start_menu_open = 0;
                }
            } else {
                if (start_menu_open) start_menu_open = 0; // Clicked outside
                
                // Window Management (Reverse order to click top-most first)
                for(int i = window_count - 1; i >= 0; i--) {
                    window_t* win = &windows[i];
                    if (mouse_x >= win->x && mouse_x <= win->x + win->w &&
                        mouse_y >= win->y && mouse_y <= win->y + win->h) {
                        
                        // Bring to front
                        open_window(win->type); // Re-opening essentially focuses it in our logic
                        win = &windows[window_count - 1]; // Update pointer to new location

                        // Check Close Button (Top right approx 20x20)
                        if (mouse_x >= win->x + win->w - 25 && mouse_y <= win->y + 25) {
                            close_window(window_count - 1);
                        }
                        // Check Title Bar (Dragging)
                        else if (mouse_y <= win->y + 25) {
                            win->dragging = 1;
                        }
                        // Check Settings Buttons
                        else if (win->type == MODE_SETTINGS) {
                             if (mouse_x >= win->x + 120 && mouse_x <= win->x + 220 &&
                                 mouse_y >= win->y + 45 && mouse_y <= win->y + 65) {
                                 wallpaper_mode = !wallpaper_mode;
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
            if (active->dragging) {
                active->x += mouse_dx;
                active->y += mouse_dy;
            }
        } else if (!mouse_left && window_count > 0) {
            windows[window_count - 1].dragging = 0;
        }

        prev_mouse_left = mouse_left;
        prev_mouse_x = mouse_x;
        prev_mouse_y = mouse_y;

        // 1. Clear Screen / Draw Background
        draw_desktop(wallpaper_mode);
        draw_taskbar();

        // 2. Draw UI Elements
        for(int i = 0; i < window_count; i++) {
            draw_window(&windows[i]);
        }
        if (start_menu_open) draw_start_menu();

        // 3. Draw Mouse Cursor (last, so it stays on top)
        draw_cursor(mouse_x, mouse_y);

        // 4. Render the frame
        screen_update();

        // 5. Wait for interrupt (cools down CPU and syncs with mouse IRQ)
        asm volatile("hlt");
    }
}