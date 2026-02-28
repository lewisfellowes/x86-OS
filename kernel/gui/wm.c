#include "gui/wm.h"
#include "gui/widget.h"
#include "drivers/fb.h"
#include "drivers/kbd.h"
#include "gfx/font.h"
#include "lib/string.h"
#include "drivers/serial.h"

#define MAX_ZORDER 16

static window_t *zorder[MAX_ZORDER];
static int       zcount;
static int       drag_active;
static int       drag_ox, drag_oy;
static window_t *drag_win;

void wm_init(void) {
    zcount = 0;
    drag_active = 0;
    drag_win = 0;
}

void wm_add_window(window_t *win) {
    if (zcount >= MAX_ZORDER) return;
    zorder[zcount++] = win;
    wm_focus(win);
}

void wm_remove_window(window_t *win) {
    for (int i = 0; i < zcount; i++) {
        if (zorder[i] == win) {
            for (int j = i; j < zcount - 1; j++)
                zorder[j] = zorder[j + 1];
            zcount--;
            return;
        }
    }
}

void wm_focus(window_t *win) {
    if (!win) return;

    /* Move to top of z-order */
    for (int i = 0; i < zcount; i++) {
        zorder[i]->focused = false;
        if (zorder[i] == win) {
            for (int j = i; j < zcount - 1; j++)
                zorder[j] = zorder[j + 1];
            zorder[zcount - 1] = win;
        }
    }
    win->focused = true;
}

static window_t *find_window_at(int mx, int my) {
    for (int i = zcount - 1; i >= 0; i--) {
        window_t *w = zorder[i];
        if (!w->visible) continue;
        int wx = w->x - WIN_BORDER;
        int wy = w->y - WIN_TITLE_H;
        int ww = w->w + WIN_BORDER * 2;
        int wh = w->h + WIN_TITLE_H + WIN_BORDER;
        if (mx >= wx && mx < wx + ww && my >= wy && my < wy + wh)
            return w;
    }
    return 0;
}

static bool in_title_bar(window_t *w, int mx, int my) {
    return mx >= w->x && mx < w->x + w->w &&
           my >= w->y - WIN_TITLE_H && my < w->y;
}

static bool in_close_button(window_t *w, int mx, int my) {
    int bx = w->x + w->w - 18;
    int by = w->y - WIN_TITLE_H + 4;
    return mx >= bx && mx < bx + 16 && my >= by && my < by + 16;
}

bool wm_handle_event(const event_t *ev) {
    if (ev->type == EVENT_MOUSE_DOWN) {
        window_t *hit = find_window_at(ev->mouse.x, ev->mouse.y);
        if (hit) {
            wm_focus(hit);
            if (in_close_button(hit, ev->mouse.x, ev->mouse.y)) {
                wm_remove_window(hit);
                window_close(hit);
                return true;
            }
            if (in_title_bar(hit, ev->mouse.x, ev->mouse.y)) {
                drag_active = 1;
                drag_win = hit;
                drag_ox = ev->mouse.x - hit->x;
                drag_oy = ev->mouse.y - hit->y;
                return true;
            }
            if (hit->on_event) {
                int lx = ev->mouse.x - hit->x;
                int ly = ev->mouse.y - hit->y;
                hit->on_event(hit, WIN_EVENT_MOUSE_DOWN, lx, ly);
            }
            return true;
        }
        return false;
    }
    if (ev->type == EVENT_MOUSE_UP) {
        if (drag_active) {
            drag_active = 0;
            drag_win = 0;
        }
        window_t *hit = find_window_at(ev->mouse.x, ev->mouse.y);
        if (hit && hit->on_event) {
            int lx = ev->mouse.x - hit->x;
            int ly = ev->mouse.y - hit->y;
            hit->on_event(hit, WIN_EVENT_MOUSE_UP, lx, ly);
            return true;
        }
        return false;
    }
    if (ev->type == EVENT_MOUSE_MOVE) {
        if (drag_active && drag_win) {
            window_move(drag_win,
                        ev->mouse.x - drag_ox,
                        ev->mouse.y - drag_oy);
        }
        return false;
    }
    if (ev->type == EVENT_KEY_DOWN || ev->type == EVENT_KEY_UP) {
        if (ev->type == EVENT_KEY_DOWN &&
            ev->key.scancode == 0x3E &&
            (kbd_get_modifiers() & KBD_MOD_ALT)) {
            if (zcount > 0) {
                window_t *top = zorder[zcount - 1];
                wm_remove_window(top);
                window_close(top);
                return true;
            }
        }

        if (zcount > 0 && zorder[zcount - 1]->on_event) {
            int t = (ev->type == EVENT_KEY_DOWN) ? WIN_EVENT_KEY_DOWN : WIN_EVENT_KEY_UP;
            zorder[zcount - 1]->on_event(zorder[zcount - 1], t,
                                          ev->key.scancode, ev->key.ascii);
            return true;
        }
        return false;
    }
    return false;
}

static void draw_window_frame(window_t *w) {
    uint32_t title_bg = w->focused ? 0x002050A0 : 0x00606878;

    /* Shadow */
    fb_fill_rect(w->x + 3, w->y - WIN_TITLE_H + 3,
                 w->w + WIN_BORDER * 2, w->h + WIN_TITLE_H + WIN_BORDER,
                 0x00101820);

    /* Border */
    fb_fill_rect(w->x - WIN_BORDER, w->y - WIN_TITLE_H,
                 w->w + WIN_BORDER * 2, w->h + WIN_TITLE_H + WIN_BORDER,
                 0x001A3050);

    /* Title bar */
    fb_fill_rect(w->x, w->y - WIN_TITLE_H, w->w, WIN_TITLE_H, title_bg);
    font_draw_string(w->x + 8, w->y - WIN_TITLE_H + 4,
                     w->title, 0x00FFFFFF, title_bg);

    /* Close button */
    int bx = w->x + w->w - 18;
    int by = w->y - WIN_TITLE_H + 4;
    fb_fill_rect(bx, by, 16, 16, 0x00E04040);
    font_draw_string(bx + 4, by, "X", 0x00FFFFFF, 0x00E04040);

    /* Client area background */
    fb_fill_rect(w->x, w->y, w->w, w->h, 0x00E8E8E8);
}

void wm_compose(void) {
    for (int i = 0; i < zcount; i++) {
        window_t *w = zorder[i];
        if (!w->visible) continue;

        draw_window_frame(w);

        if (w->on_draw)
            w->on_draw(w);

        w->dirty = false;
    }
}

window_t *wm_get_window(int index) {
    if (index < 0 || index >= zcount) return 0;
    return zorder[index];
}

int wm_window_count(void) { return zcount; }
