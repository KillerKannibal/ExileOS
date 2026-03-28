#include "shell.h"
#include "string.h"
#include "fs.h"
#include "wm.h"
#include "editor.h"
#include "globals.h"
#include "utils.h"
#include "io.h"
#include "gui.h"

char term_buffer[MAX_TERM_LINES][MAX_TERM_COLS];
char keyboard_buffer[256];
int kb_buffer_len = 0;

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
        term_print("  fetch - Display system info");
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
        char ver_str[40];
        strcpy(ver_str, KERNEL_NAME);
        strcat(ver_str, " Kernel v");
        strcat(ver_str, KERNEL_VERSION);
        term_print(ver_str);
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
    else if (strcmp(input, "fetch") == 0) {
        #define MAX_ART_LINES 16
        #define MAX_INFO_LINES 5

        fs_node_t* node = finddir_fs(fs_root, ".config/fetch/minfetch.conf");
        char art_buffer[1024];
        char* art_lines[MAX_ART_LINES] = {0};
        int art_line_count = 0;

        if (node) {
            uint32_t size = read_fs(node, 0, sizeof(art_buffer)-1, (uint8_t*)art_buffer);
            art_buffer[size] = '\0';

            char* p = art_buffer;
            while (art_line_count < MAX_ART_LINES && p) {
                art_lines[art_line_count++] = p;
                char* newline = p;
                while (*newline != '\n' && *newline != '\0') {
                    newline++;
                }
                if (*newline == '\n') {
                    if (p == newline && *(p+1) == '\0') break; // Handle trailing newline
                    *newline = '\0';
                    p = newline + 1;
                } else {
                    p = NULL;
                }
            }
        }

        char info_lines[MAX_INFO_LINES][64];
        char temp_val[20];

        // Prepare info lines
        strcpy(info_lines[0], "OS: ");
        strcat(info_lines[0], KERNEL_NAME);
        strcat(info_lines[0], " Kernel v");
        strcat(info_lines[0], KERNEL_VERSION);

        strcpy(info_lines[1], "CPU: 1 Core (");
        strcat(info_lines[1], KERNEL_ARCH);
        strcat(info_lines[1], ")");

        int total_mem = 0;
        if (global_mbinfo) total_mem = (global_mbinfo->mem_upper / 1024) + 1;
        simple_itoa(total_mem, temp_val);
        strcpy(info_lines[2], "Mem: ");
        strcat(info_lines[2], temp_val);
        strcat(info_lines[2], " MB");

        strcpy(info_lines[3], "Res: ");
        simple_itoa(screen_width, temp_val);
        strcat(info_lines[3], temp_val);
        strcat(info_lines[3], "x");
        simple_itoa(screen_height, temp_val);
        strcat(info_lines[3], temp_val);

        strcpy(info_lines[4], "Dev: ");
        strcat(info_lines[4], KERNEL_VENDOR);

        // Print interleaved
        char line_buf[MAX_TERM_COLS];
        int max_lines = (art_line_count > MAX_INFO_LINES) ? art_line_count : MAX_INFO_LINES;
        int info_start_line = (art_line_count > MAX_INFO_LINES) ? (art_line_count - MAX_INFO_LINES) / 2 : 0;

        for (int i = 0; i < max_lines; i++) {
            line_buf[0] = '\0';

            // Add art part
            if (i < art_line_count && art_lines[i]) {
                strcpy(line_buf, art_lines[i]);
            }

            // Add info part
            int info_idx = i - info_start_line;
            if (info_idx >= 0 && info_idx < MAX_INFO_LINES) {
                // Add padding if there is art on this line
                if (i < art_line_count && art_lines[i] && strlen(art_lines[i]) > 0) {
                    strcat(line_buf, " ");
                }
                strcat(line_buf, info_lines[info_idx]);
            }
            term_print(line_buf);
        }
    }
    else {
        term_print("Unknown command.");
    }
}