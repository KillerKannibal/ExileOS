#include "editor.h"
#include "fs.h"
#include "string.h"
#include "wm.h"

char editor_buffer[EDITOR_MAX_LINES][EDITOR_MAX_COLS];
int editor_num_lines = 0;
int editor_cursor_x = 0;
int editor_cursor_y = 0;
char editor_filename[128];

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