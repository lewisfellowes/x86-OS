#include "gui/window.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/fb.h"
#include "drivers/kbd.h"
#include "fs/fs.h"
#include "lib/string.h"
#include "mem/heap.h"
#include "drivers/serial.h"

#define ED_W       480
#define ED_H       360
#define ED_COLS    ((ED_W - 16) / FONT_CHAR_WIDTH)
#define ED_ROWS    ((ED_H - 40) / (FONT_CHAR_HEIGHT + 2))
#define ED_BUF_MAX 8192

#define SC_UP     0x48
#define SC_DOWN   0x50
#define SC_LEFT   0x4B
#define SC_RIGHT  0x4D
#define SC_HOME   0x47
#define SC_END    0x4F
#define SC_PGUP   0x49
#define SC_PGDN   0x51
#define SC_CTRL   0x1D
#define SC_S      0x1F

typedef struct {
    char     buf[ED_BUF_MAX];
    int      len;
    int      cursor;
    int      scroll_line;
    char     filename[FS_MAX_NAME];
    bool     modified;
    bool     ctrl_held;
} editor_state_t;

static editor_state_t es;
static window_t *ed_win;

static int ed_line_count(void) {
    int lines = 1;
    for (int i = 0; i < es.len; i++)
        if (es.buf[i] == '\n') lines++;
    return lines;
}

static int ed_cursor_line(void) {
    int line = 0;
    for (int i = 0; i < es.cursor && i < es.len; i++)
        if (es.buf[i] == '\n') line++;
    return line;
}

static int ed_cursor_col(void) {
    int col = 0;
    for (int i = es.cursor - 1; i >= 0; i--) {
        if (es.buf[i] == '\n') break;
        col++;
    }
    return col;
}

static int ed_line_start(int line) {
    if (line == 0) return 0;
    int cur = 0;
    for (int i = 0; i < es.len; i++) {
        if (es.buf[i] == '\n') {
            cur++;
            if (cur == line) return i + 1;
        }
    }
    return es.len;
}

static int ed_line_end(int line) {
    int start = ed_line_start(line);
    for (int i = start; i < es.len; i++)
        if (es.buf[i] == '\n') return i;
    return es.len;
}

static void ed_ensure_visible(void) {
    int cur_line = ed_cursor_line();
    if (cur_line < es.scroll_line)
        es.scroll_line = cur_line;
    if (cur_line >= es.scroll_line + ED_ROWS)
        es.scroll_line = cur_line - ED_ROWS + 1;
}

static void ed_insert_char(char c) {
    if (es.len >= ED_BUF_MAX - 1) return;
    memmove(es.buf + es.cursor + 1, es.buf + es.cursor,
            (size_t)(es.len - es.cursor));
    es.buf[es.cursor] = c;
    es.len++;
    es.cursor++;
    es.modified = true;
}

static void ed_delete_back(void) {
    if (es.cursor <= 0) return;
    es.cursor--;
    memmove(es.buf + es.cursor, es.buf + es.cursor + 1,
            (size_t)(es.len - es.cursor - 1));
    es.len--;
    es.modified = true;
}

static void ed_save(void) {
    if (es.filename[0] == '\0') return;

    fd_t fd = fs_open(es.filename, FS_FLAG_WRITE | FS_FLAG_CREATE);
    if (fd < 0) {
        serial_puts("ED: save failed (open)\r\n");
        return;
    }
    fs_seek(fd, 0);
    int written = fs_write(fd, es.buf, (uint32_t)es.len);
    fs_close(fd);

    if (written >= 0) {
        es.modified = false;
        serial_puts("ED: saved '");
        serial_puts(es.filename);
        serial_puts("'\r\n");
    } else {
        serial_puts("ED: save failed (write)\r\n");
    }
}

static void ed_draw(window_t *w) {
    uint32_t bg   = 0x00FFFFF8;
    uint32_t fg   = 0x00202020;
    uint32_t lnbg = 0x00E8E8E0;
    uint32_t crbg = 0x00B0D0F0;

    fb_fill_rect(w->x, w->y, w->w, w->h, bg);

    int cur_line = ed_cursor_line();
    int cur_col  = ed_cursor_col();

    int line = es.scroll_line;
    int pos  = ed_line_start(line);

    for (int row = 0; row < ED_ROWS && line < ed_line_count(); row++, line++) {
        int py = w->y + 4 + row * (FONT_CHAR_HEIGHT + 2);

        /* Line number gutter */
        char ln[4];
        int l = line + 1;
        ln[0] = (l >= 100) ? ('0' + (char)(l / 100)) : ' ';
        ln[1] = (l >= 10)  ? ('0' + (char)((l / 10) % 10)) : ' ';
        ln[2] = '0' + (char)(l % 10);
        ln[3] = '\0';
        font_draw_string(w->x + 2, py, ln, 0x00909090, lnbg);
        fb_fill_rect(w->x + 28, py, 1, FONT_CHAR_HEIGHT, 0x00C0C0C0);

        int col = 0;
        while (pos < es.len && es.buf[pos] != '\n') {
            if (col < ED_COLS) {
                int px = w->x + 32 + col * FONT_CHAR_WIDTH;
                uint32_t cbg = (line == cur_line && col == cur_col) ? crbg : bg;
                font_draw_char(px, py, es.buf[pos], fg, cbg);
            }
            col++;
            pos++;
        }

        /* Draw cursor at end of line */
        if (line == cur_line && cur_col == col && col < ED_COLS) {
            int px = w->x + 32 + col * FONT_CHAR_WIDTH;
            fb_fill_rect(px, py, FONT_CHAR_WIDTH, FONT_CHAR_HEIGHT, crbg);
        }

        if (pos < es.len) pos++; /* skip newline */
    }

    /* Status bar */
    int sy = w->y + w->h - 20;
    fb_fill_rect(w->x, sy, w->w, 20, 0x00304050);

    char status[64];
    int sp = 0;
    if (es.filename[0]) {
        for (int i = 0; es.filename[i] && sp < 28; i++)
            status[sp++] = es.filename[i];
    } else {
        const char *u = "untitled";
        while (*u) status[sp++] = *u++;
    }
    if (es.modified) status[sp++] = '*';
    status[sp++] = ' ';
    status[sp++] = 'L';
    if (cur_line + 1 >= 10) status[sp++] = '0' + (char)((cur_line + 1) / 10);
    status[sp++] = '0' + (char)((cur_line + 1) % 10);
    status[sp++] = ':';
    status[sp++] = 'C';
    if (cur_col >= 10) status[sp++] = '0' + (char)(cur_col / 10);
    status[sp++] = '0' + (char)(cur_col % 10);
    status[sp++] = ' ';
    status[sp++] = ' ';
    const char *hint = "Ctrl+S save";
    while (*hint && sp < 60) status[sp++] = *hint++;
    status[sp] = '\0';

    font_draw_string(w->x + 8, sy + 2, status, 0x00D0D8E0, 0x00304050);
}

static void ed_event(window_t *w, int type, int d1, int d2) {
    (void)w; (void)d2;

    if (type == WIN_EVENT_KEY_DOWN) {
        uint8_t sc = (uint8_t)d1;

        if (sc == SC_CTRL) { es.ctrl_held = true; return; }

        if (es.ctrl_held && sc == SC_S) {
            ed_save();
            window_set_dirty(ed_win);
            return;
        }

        switch (sc) {
        case SC_UP: {
            int line = ed_cursor_line();
            if (line > 0) {
                int col = ed_cursor_col();
                int prev_start = ed_line_start(line - 1);
                int prev_end   = ed_line_end(line - 1);
                int prev_len   = prev_end - prev_start;
                es.cursor = prev_start + (col < prev_len ? col : prev_len);
            }
            break;
        }
        case SC_DOWN: {
            int line = ed_cursor_line();
            if (line < ed_line_count() - 1) {
                int col = ed_cursor_col();
                int next_start = ed_line_start(line + 1);
                int next_end   = ed_line_end(line + 1);
                int next_len   = next_end - next_start;
                es.cursor = next_start + (col < next_len ? col : next_len);
            }
            break;
        }
        case SC_LEFT:
            if (es.cursor > 0) es.cursor--;
            break;
        case SC_RIGHT:
            if (es.cursor < es.len) es.cursor++;
            break;
        case SC_HOME:
            es.cursor = ed_line_start(ed_cursor_line());
            break;
        case SC_END:
            es.cursor = ed_line_end(ed_cursor_line());
            break;
        case SC_PGUP:
            es.scroll_line -= ED_ROWS;
            if (es.scroll_line < 0) es.scroll_line = 0;
            es.cursor = ed_line_start(es.scroll_line);
            break;
        case SC_PGDN: {
            int total = ed_line_count();
            es.scroll_line += ED_ROWS;
            if (es.scroll_line >= total) es.scroll_line = total - 1;
            es.cursor = ed_line_start(es.scroll_line);
            break;
        }
        default: {
            char ascii = kbd_scancode_to_ascii(sc);
            if (ascii == '\b') {
                ed_delete_back();
            } else if (ascii == '\n') {
                ed_insert_char('\n');
            } else if (ascii == '\t') {
                for (int i = 0; i < 4; i++) ed_insert_char(' ');
            } else if (ascii >= ' ' && ascii <= '~') {
                ed_insert_char(ascii);
            }
            break;
        }
        }

        ed_ensure_visible();
        window_set_dirty(ed_win);
    } else if (type == WIN_EVENT_KEY_UP) {
        uint8_t sc = (uint8_t)d1;
        if (sc == SC_CTRL) es.ctrl_held = false;
    }
}

void app_editor_open_file(const char *path) {
    memset(&es, 0, sizeof(es));

    if (path && path[0]) {
        strncpy(es.filename, path, FS_MAX_NAME - 1);
        fd_t fd = fs_open(path, FS_FLAG_READ);
        if (fd >= 0) {
            es.len = fs_read(fd, es.buf, ED_BUF_MAX - 1);
            if (es.len < 0) es.len = 0;
            fs_close(fd);
        }
    }

    ed_win = window_create("Editor", ED_W, ED_H);
    if (!ed_win) return;

    if (es.filename[0]) {
        char title[32] = "Editor - ";
        int tp = 9;
        for (int i = 0; es.filename[i] && tp < 30; i++)
            title[tp++] = es.filename[i];
        title[tp] = '\0';
        window_set_title(ed_win, title);
    }

    ed_win->on_draw  = ed_draw;
    ed_win->on_event = ed_event;
    wm_add_window(ed_win);
}

void app_editor_open(void) {
    app_editor_open_file((void *)0);
}
