#pragma once
#include <stdint.h>
#include <stdbool.h>

#define WIN_MAX       16
#define WIN_TITLE_H   24
#define WIN_BORDER    1

struct window;
typedef void (*win_draw_fn)(struct window *w);
typedef void (*win_event_fn)(struct window *w, int type, int data1, int data2);

typedef struct window {
    int16_t  x, y, w, h;
    char     title[32];
    uint32_t *backbuf;
    uint32_t  pid;
    bool      visible;
    bool      dirty;
    bool      focused;
    win_draw_fn  on_draw;
    win_event_fn on_event;
    void        *userdata;
} window_t;

#define WIN_EVENT_KEY_DOWN   1
#define WIN_EVENT_KEY_UP     2
#define WIN_EVENT_MOUSE_DOWN 3
#define WIN_EVENT_MOUSE_UP   4
#define WIN_EVENT_MOUSE_MOVE 5

window_t *window_create(const char *title, int w, int h);
void      window_close(window_t *win);
void      window_move(window_t *win, int x, int y);
void      window_set_dirty(window_t *win);
void      window_set_title(window_t *win, const char *title);
