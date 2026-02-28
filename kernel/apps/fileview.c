#include "gui/window.h"
#include "gui/wm.h"
#include "gui/widget.h"
#include "gfx/font.h"
#include "drivers/fb.h"
#include "fs/fs.h"
#include "lib/string.h"
#include "mem/heap.h"
#include "drivers/serial.h"

#define FV_W 420
#define FV_H 340
#define FV_LIST_H 120
#define FV_ENTRY_H 18

typedef struct {
    fs_dirent_t entries[FS_MAX_DIRENT];
    int         count;
    int         selected;
    char       *file_buf;
    int         file_len;
} fv_state_t;

static fv_state_t fv;
static window_t *fv_win;

static void fv_load_file(int idx) {
    if (fv.file_buf) { kfree(fv.file_buf); fv.file_buf = 0; }
    fv.file_len = 0;

    fs_stat_t st;
    if (fs_stat(fv.entries[idx].name, &st) != 0) return;

    fv.file_buf = (char *)kmalloc(st.size + 1);
    if (!fv.file_buf) return;

    fd_t fd = fs_open(fv.entries[idx].name, FS_FLAG_READ);
    if (fd < 0) { kfree(fv.file_buf); fv.file_buf = 0; return; }
    fv.file_len = fs_read(fd, fv.file_buf, st.size);
    fs_close(fd);
    if (fv.file_len >= 0) fv.file_buf[fv.file_len] = '\0';
}

static void fv_draw(window_t *w) {
    fb_fill_rect(w->x, w->y, w->w, FV_LIST_H, 0x00F0F0F0);

    for (int i = 0; i < fv.count; i++) {
        int ey = w->y + i * FV_ENTRY_H;
        if (ey > w->y + FV_LIST_H - FV_ENTRY_H) break;
        uint32_t bg = (i == fv.selected) ? 0x003070B0 : 0x00F0F0F0;
        uint32_t fg = (i == fv.selected) ? 0x00FFFFFF : 0x00303030;
        fb_fill_rect(w->x, ey, w->w, FV_ENTRY_H, bg);
        fb_fill_rect(w->x + 4, ey + 2, 10, 14, 0x00E8C840);
        font_draw_string(w->x + 20, ey + 1, fv.entries[i].name, fg, bg);
    }

    /* Separator */
    fb_fill_rect(w->x, w->y + FV_LIST_H, w->w, 2, 0x00A0A0B0);

    /* Content area */
    int cy = w->y + FV_LIST_H + 2;
    int ch = w->h - FV_LIST_H - 2;
    fb_fill_rect(w->x, cy, w->w, ch, 0x00FFFFF0);

    if (fv.file_buf) {
        const char *p = fv.file_buf;
        int px = w->x + 4;
        int py = cy + 4;
        while (*p && py < w->y + w->h - FONT_CHAR_HEIGHT) {
            if (*p == '\n') {
                px = w->x + 4;
                py += FONT_CHAR_HEIGHT + 2;
            } else {
                if (px > w->x + w->w - FONT_CHAR_WIDTH - 4) {
                    px = w->x + 4;
                    py += FONT_CHAR_HEIGHT + 2;
                }
                font_draw_char(px, py, *p, 0x00303030, 0x00FFFFF0);
                px += FONT_CHAR_WIDTH;
            }
            p++;
        }
    }
}

static void fv_event(window_t *w, int type, int d1, int d2) {
    (void)w;
    if (type != WIN_EVENT_MOUSE_DOWN) return;

    int ly = d2;
    if (ly >= 0 && ly < FV_LIST_H) {
        int idx = ly / FV_ENTRY_H;
        if (idx >= 0 && idx < fv.count && idx != fv.selected) {
            fv.selected = idx;
            fv_load_file(idx);
            window_set_dirty(fv_win);
        }
    }
    (void)d1;
}

void app_fileview_open(void) {
    memset(&fv, 0, sizeof(fv));
    fv.count = fs_readdir("/", fv.entries, FS_MAX_DIRENT);
    fv.selected = -1;

    if (fv.count > 0) {
        fv.selected = 0;
        fv_load_file(0);
    }

    fv_win = window_create("File Browser", FV_W, FV_H);
    if (!fv_win) return;

    fv_win->on_draw  = fv_draw;
    fv_win->on_event = fv_event;

    wm_add_window(fv_win);
}
