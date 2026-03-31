#ifndef WM_H
#define WM_H

#include <stdint.h>

#define MAX_WINDOWS 10
#define TITLE_LEN   64

typedef enum {
    MODE_SHELL,
    MODE_EDITOR,
    MODE_CALC,
    MODE_SYSINFO,
    MODE_MONITOR,
    MODE_SETTINGS,
    MODE_FILEMAN,
    MODE_GAME,
    MODE_BROWSER
} term_mode_t;

typedef struct {
    int x, y, w, h;
    term_mode_t type;
    char title[TITLE_LEN];
    int dragging;
    int minimized;
    int fullscreen;
    int old_x, old_y, old_w, old_h;
    int resizing;
} window_t;

extern window_t windows[MAX_WINDOWS];
extern int window_count;

void open_window(term_mode_t type);
void close_window(int index);
void draw_window(window_t* win);
void draw_window_buttons(window_t* win, int x, int y, int w);
void wm_update();
void wm_draw_all();

#endif