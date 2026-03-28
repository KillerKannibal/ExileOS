#include "input.h"
#include "io.h"
#include "wm.h"
#include "editor.h"
#include "calc.h"
#include "game.h"
#include "shell.h"
#include "string.h"
#include "gui.h"
#include "utils.h"

volatile int mouse_x = 512;
volatile int mouse_y = 384;
volatile uint8_t mouse_left = 0;
volatile uint8_t mouse_cycle = 0;
uint8_t mouse_byte[3];

uint8_t shift_pressed = 0;
uint8_t ctrl_pressed = 0;

void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;

    if (type == 0) {
        while (timeout--) {
            if (inb(0x64) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(0x64) & 2)) return;
        }
    }
}

void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, data);
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

    status = inb(0x60);
    status |= 2;

    mouse_wait(1);
    outb(0x64, 0x60);

    mouse_wait(1);
    outb(0x60, status);

    mouse_write(0xF6);
    mouse_read();

    mouse_write(0xF4);
    mouse_read();
}

void mouse_update(int dx, int dy, uint8_t buttons) {
    mouse_left = buttons & 0x01;
    mouse_x += dx;
    mouse_y += dy;

    // Clamp to screen bounds
    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;

    if (mouse_x >= (int)screen_width) mouse_x = screen_width - 1;
    if (mouse_y >= (int)screen_height) mouse_y = screen_height - 1;
}

void mouse_handler() {

    uint8_t data = inb(0x60);
    // Send EOI immediately to both PICs for every byte received
    outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (mouse_cycle == 0) {
        if (data & 0x08) {
            mouse_byte[0] = data;
            mouse_cycle++;
        }
        return;
    }

    mouse_byte[mouse_cycle++] = data;

    if (mouse_cycle < 3) return;

    mouse_cycle = 0;

    int x_move = mouse_byte[1];
    int y_move = mouse_byte[2];

    if (mouse_byte[0] & 0x10) x_move |= 0xFFFFFF00;
    if (mouse_byte[0] & 0x20) y_move |= 0xFFFFFF00;

    mouse_update(x_move, -y_move, mouse_byte[0]);
}

// --- Keyboard Maps ---

unsigned char keyboard_map[128] = {
0,27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
'\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
0,'a','s','d','f','g','h','j','k','l',';','\'','`',
0,'\\','z','x','c','v','b','n','m',',','.','/',0,
'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0
};

unsigned char keyboard_map_shifted[128] = {
0,27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
'\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
0,'A','S','D','F','G','H','J','K','L',':','"','~',
0,'|','Z','X','C','V','B','N','M','<','>','?',0,
'*',0,' ',0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
'-',0,0,0,'+',0,0,0,0,0,0,0,0,0,0,0
};

void keyboard_handler() {
    uint8_t scancode = inb(0x60);
    // Send EOI immediately so we don't miss it in any exit paths
    outb(0x20, 0x20);

    // Handle key releases (Break codes)
    if (scancode & 0x80) {
        uint8_t released = scancode & 0x7F;
        if (released == 0x2A || released == 0x36) shift_pressed = 0;
        if (released == 0x1D) ctrl_pressed = 0;
        return;
    }

    // Handle key presses (Make codes) for modifiers
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x1D) { ctrl_pressed = 1; return; }

    char c = shift_pressed ? keyboard_map_shifted[scancode] : keyboard_map[scancode];

    if (window_count == 0) return;

    window_t* active_win = &windows[window_count - 1];

    // ---------- EDITOR ----------
    if (active_win->type == MODE_EDITOR) {

        char* line = editor_buffer[editor_cursor_y];
        int line_len = strlen(line);

        if (c == '\b') {
            if (editor_cursor_x > 0) {
                memmove(&line[editor_cursor_x - 1], &line[editor_cursor_x],
                        line_len - editor_cursor_x + 1);
                editor_cursor_x--;
            }
            else if (editor_cursor_y > 0) {
                // Merge current line with previous line
                char* prev_line = editor_buffer[editor_cursor_y - 1];
                int prev_len = strlen(prev_line);
                
                if (prev_len + line_len < EDITOR_MAX_COLS - 1) {
                    strcat(prev_line, line);
                    // Shift remaining lines up
                    for (int i = editor_cursor_y; i < editor_num_lines - 1; i++)
                        strcpy(editor_buffer[i], editor_buffer[i + 1]);
                    editor_num_lines--;
                    editor_cursor_y--;
                    editor_cursor_x = prev_len;
                }
            }
        }
        else if (c == '\n') {

            if (editor_num_lines < EDITOR_MAX_LINES - 1) {

                for (int i = editor_num_lines; i > editor_cursor_y + 1; i--)
                    strcpy(editor_buffer[i], editor_buffer[i - 1]);

                strcpy(editor_buffer[editor_cursor_y + 1],
                       &line[editor_cursor_x]);

                line[editor_cursor_x] = '\0';

                editor_cursor_y++;
                editor_cursor_x = 0;
                editor_num_lines++;
            }
        }
        else if (c != 0) {

            if (line_len < EDITOR_MAX_COLS - 1) {

                memmove(&line[editor_cursor_x + 1],
                        &line[editor_cursor_x],
                        line_len - editor_cursor_x + 1);

                line[editor_cursor_x] = c;
                editor_cursor_x++;
            }
        }

        return;
    }

    // ---------- GAME ----------
    if (active_win->type == MODE_GAME) {

        int moveSpeed = 8000;

        if (c == 'w') {
            int nextX = p_x + (int)(((int64_t)p_dir_x * moveSpeed) >> 16);
            int nextY = p_y + (int)(((int64_t)p_dir_y * moveSpeed) >> 16);

            int gridX = TO_INT(nextX);
            int gridY = TO_INT(nextY);

            // Bounds check and wall collision (sliding)
            if (gridX >= 0 && gridX < MAP_W && gridY >= 0 && gridY < MAP_H) {
                if (world_map[gridX][TO_INT(p_y)] == 0) p_x = nextX;
                if (world_map[TO_INT(p_x)][gridY] == 0) p_y = nextY;
            }
        }

        if (c == 's') {
            int nextX = p_x - (int)(((int64_t)p_dir_x * moveSpeed) >> 16);
            int nextY = p_y - (int)(((int64_t)p_dir_y * moveSpeed) >> 16);

            int gridX = TO_INT(nextX);
            int gridY = TO_INT(nextY);

            if (gridX >= 0 && gridX < MAP_W && gridY >= 0 && gridY < MAP_H) {
                if (world_map[gridX][TO_INT(p_y)] == 0) p_x = nextX;
                if (world_map[TO_INT(p_x)][gridY] == 0) p_y = nextY;
            }
        }

        return;
    }

    // ---------- CALCULATOR ----------
    if (active_win->type == MODE_CALC) {

        if (c == '\b') {
            if (calc_len > 0)
                calc_buffer[--calc_len] = '\0';
        }

        else if (c == '\n') {

            int op_idx = -1;
            char op = 0;

            for (int i = 0; i < calc_len; i++) {

                if (calc_buffer[i] == '+' ||
                    calc_buffer[i] == '-' ||
                    calc_buffer[i] == '*' ||
                    calc_buffer[i] == '/') {

                    op_idx = i;
                    op = calc_buffer[i];
                    break;
                }
            }

            if (op_idx != -1) {

                char left[32], right[32];

                strcpy(left, calc_buffer);
                left[op_idx] = '\0';

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

        else if (c != 0) {

            if (calc_len < 30) {

                calc_buffer[calc_len++] = c;
                calc_buffer[calc_len] = '\0';
            }
        }

        return;
    }

    // ---------- SHELL ----------
    if (active_win->type == MODE_SHELL) {

        if (c == '\b') {
            if (kb_buffer_len > 0) {
                kb_buffer_len--;
                // Null terminate at new length so display/exec works correctly
                keyboard_buffer[kb_buffer_len] = '\0';
            }
        }

        else if (c == '\n') {

            keyboard_buffer[kb_buffer_len] = '\0';
            exec_command(keyboard_buffer);

            kb_buffer_len = 0;
        }

        else if (c != 0) {

            if (kb_buffer_len < 256 - 1) {

                keyboard_buffer[kb_buffer_len++] = c;
                keyboard_buffer[kb_buffer_len] = '\0';
            }
        }
    }
}