#ifndef EDITOR_H
#define EDITOR_H

#define EDITOR_MAX_LINES 100
#define EDITOR_MAX_COLS  70 // Must be <= MAX_TERM_COLS

extern char editor_buffer[EDITOR_MAX_LINES][EDITOR_MAX_COLS];
extern int editor_num_lines;
extern int editor_cursor_x;
extern int editor_cursor_y;
extern char editor_filename[128];

void start_editor(const char* filename);

#endif