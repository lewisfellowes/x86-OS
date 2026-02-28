#include "window.h"
#include "../heap.h"
#include "../string.h"
#include "../serial.h"

static window_t windows[WIN_MAX];

window_t *window_create(const char *title, int w, int h) {
    for (int i = 0; i < WIN_MAX; i++) {
        if (!windows[i].visible) {
            window_t *win = &windows[i];
            strncpy(win->title, title, 31);
            win->title[31] = '\0';
            win->w = (int16_t)w;
            win->h = (int16_t)h;
            win->x = (int16_t)(60 + i * 20);
            win->y = (int16_t)(40 + i * 15);
            win->visible = true;
            win->dirty   = true;
            win->focused = false;
            win->on_draw  = 0;
            win->on_event = 0;
            win->userdata = 0;
            win->pid = 0;

            win->backbuf = (uint32_t *)kmalloc((uint32_t)(w * h * 4));
            if (win->backbuf)
                memset(win->backbuf, 0, (uint32_t)(w * h * 4));

            serial_puts("WIN: created '");
            serial_puts(title);
            serial_puts("'\r\n");
            return win;
        }
    }
    return 0;
}

void window_close(window_t *win) {
    if (!win) return;
    win->visible = false;
    if (win->backbuf) {
        kfree(win->backbuf);
        win->backbuf = 0;
    }
    serial_puts("WIN: closed '");
    serial_puts(win->title);
    serial_puts("'\r\n");
}

void window_move(window_t *win, int x, int y) {
    if (!win) return;
    win->x = (int16_t)x;
    win->y = (int16_t)y;
    win->dirty = true;
}

void window_set_dirty(window_t *win) {
    if (win) win->dirty = true;
}

void window_set_title(window_t *win, const char *title) {
    if (!win) return;
    strncpy(win->title, title, 31);
    win->title[31] = '\0';
    win->dirty = true;
}
