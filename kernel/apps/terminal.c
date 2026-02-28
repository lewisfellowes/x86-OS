#include "gui/window.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/fb.h"
#include "lib/string.h"
#include "drivers/kbd.h"

#define TERM_COLS    48
#define TERM_VIS     14
#define TERM_BUF     128
#define TERM_W       (TERM_COLS * FONT_CHAR_WIDTH + 16)
#define TERM_H       (TERM_VIS * (FONT_CHAR_HEIGHT + 2) + 8)

#define HIST_MAX     16
#define SC_UP        0x48
#define SC_DOWN      0x50
#define SC_PGUP      0x49
#define SC_PGDN      0x51

typedef struct {
    char lines[TERM_BUF][TERM_COLS + 1];
    int  total_lines;
    int  cur_row;
    int  cur_col;
    int  scroll_off;
    char input[TERM_COLS + 1];
    int  input_len;
    char history[HIST_MAX][TERM_COLS + 1];
    int  hist_count;
    int  hist_idx;
    int  prompt_col;
} term_state_t;

static term_state_t ts;
static window_t *term_win;

static void term_scroll_buf(void) {
    if (ts.total_lines < TERM_BUF) {
        ts.total_lines++;
        return;
    }
    for (int i = 0; i < TERM_BUF - 1; i++)
        memcpy(ts.lines[i], ts.lines[i + 1], TERM_COLS + 1);
    memset(ts.lines[TERM_BUF - 1], 0, TERM_COLS + 1);
    if (ts.cur_row > 0) ts.cur_row--;
}

static void term_autoscroll(void) {
    if (ts.cur_row >= ts.scroll_off + TERM_VIS)
        ts.scroll_off = ts.cur_row - TERM_VIS + 1;
    if (ts.scroll_off < 0)
        ts.scroll_off = 0;
}

static void term_putchar(char c) {
    if (c == '\n') {
        ts.cur_col = 0;
        ts.cur_row++;
        if (ts.cur_row >= TERM_BUF) term_scroll_buf();
        else if (ts.cur_row >= ts.total_lines) ts.total_lines = ts.cur_row + 1;
        term_autoscroll();
        return;
    }
    if (ts.cur_col >= TERM_COLS) {
        ts.cur_col = 0;
        ts.cur_row++;
        if (ts.cur_row >= TERM_BUF) term_scroll_buf();
        else if (ts.cur_row >= ts.total_lines) ts.total_lines = ts.cur_row + 1;
    }
    ts.lines[ts.cur_row][ts.cur_col++] = c;
    if (ts.cur_row >= ts.total_lines)
        ts.total_lines = ts.cur_row + 1;
    term_autoscroll();
}

static void term_puts(const char *s) {
    while (*s) term_putchar(*s++);
}

static void hist_push(const char *cmd) {
    if (strlen(cmd) == 0) return;
    if (ts.hist_count > 0 &&
        strcmp(ts.history[(ts.hist_count - 1) % HIST_MAX], cmd) == 0)
        return;
    strncpy(ts.history[ts.hist_count % HIST_MAX], cmd, TERM_COLS);
    ts.hist_count++;
}

static void term_replace_input(const char *text) {
    /* Erase current input from display */
    for (int i = 0; i < ts.input_len; i++) {
        if (ts.cur_col > ts.prompt_col) {
            ts.cur_col--;
            ts.lines[ts.cur_row][ts.cur_col] = ' ';
        }
    }
    ts.input_len = 0;

    int len = (int)strlen(text);
    if (len > TERM_COLS - 4) len = TERM_COLS - 4;
    memcpy(ts.input, text, (size_t)len);
    ts.input[len] = '\0';
    ts.input_len = len;
    for (int i = 0; i < len; i++)
        term_putchar(text[i]);
}

static void term_process_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        term_puts("Commands: help, clear, uname, echo <text>\n");
    } else if (strcmp(cmd, "clear") == 0) {
        memset(ts.lines, 0, sizeof(ts.lines));
        ts.cur_row = 0;
        ts.cur_col = 0;
        ts.total_lines = 1;
        ts.scroll_off = 0;
    } else if (strcmp(cmd, "uname") == 0) {
        term_puts("MyOS 0.3 i686\n");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        term_puts(cmd + 5);
        term_putchar('\n');
    } else if (strlen(cmd) > 0) {
        term_puts("Unknown: ");
        term_puts(cmd);
        term_putchar('\n');
    }
}

static void term_draw(window_t *w) {
    fb_fill_rect(w->x, w->y, w->w, w->h, 0x00181818);

    for (int r = 0; r < TERM_VIS; r++) {
        int line = ts.scroll_off + r;
        if (line < 0 || line >= ts.total_lines) continue;

        int py = w->y + 4 + r * (FONT_CHAR_HEIGHT + 2);
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = ts.lines[line][c];
            if (ch)
                font_draw_char(w->x + 8 + c * FONT_CHAR_WIDTH, py,
                               ch, 0x0000FF00, 0x00181818);
        }
    }

    /* Cursor blink indicator */
    int vis_row = ts.cur_row - ts.scroll_off;
    if (vis_row >= 0 && vis_row < TERM_VIS) {
        int py = w->y + 4 + vis_row * (FONT_CHAR_HEIGHT + 2);
        int px = w->x + 8 + ts.cur_col * FONT_CHAR_WIDTH;
        font_draw_char(px, py, '_', 0x0000FF00, 0x00181818);
    }

    /* Scrollbar hint when scrolled up */
    if (ts.scroll_off + TERM_VIS < ts.total_lines) {
        int sx = w->x + w->w - 6;
        fb_fill_rect(sx, w->y + 2, 4, w->h - 4, 0x00282828);
        int bar_h = w->h * TERM_VIS / ts.total_lines;
        if (bar_h < 8) bar_h = 8;
        int bar_y = w->y + 2 + (w->h - 4 - bar_h) * ts.scroll_off /
                    (ts.total_lines - TERM_VIS > 0 ? ts.total_lines - TERM_VIS : 1);
        fb_fill_rect(sx, bar_y, 4, bar_h, 0x00508050);
    }
}

static void term_event(window_t *w, int type, int d1, int d2) {
    (void)w; (void)d2;
    if (type != WIN_EVENT_KEY_DOWN) return;

    uint8_t sc = (uint8_t)d1;

    if (sc == SC_PGUP) {
        ts.scroll_off -= TERM_VIS;
        if (ts.scroll_off < 0) ts.scroll_off = 0;
        window_set_dirty(term_win);
        return;
    }
    if (sc == SC_PGDN) {
        ts.scroll_off += TERM_VIS;
        int max_off = ts.total_lines - TERM_VIS;
        if (max_off < 0) max_off = 0;
        if (ts.scroll_off > max_off) ts.scroll_off = max_off;
        window_set_dirty(term_win);
        return;
    }
    if (sc == SC_UP) {
        if (ts.hist_count > 0) {
            if (ts.hist_idx > 0) ts.hist_idx--;
            int real = ts.hist_idx % HIST_MAX;
            term_replace_input(ts.history[real]);
            window_set_dirty(term_win);
        }
        return;
    }
    if (sc == SC_DOWN) {
        if (ts.hist_idx < ts.hist_count - 1) {
            ts.hist_idx++;
            int real = ts.hist_idx % HIST_MAX;
            term_replace_input(ts.history[real]);
        } else {
            ts.hist_idx = ts.hist_count;
            term_replace_input("");
        }
        window_set_dirty(term_win);
        return;
    }

    char ascii = kbd_scancode_to_ascii(sc);
    if (!ascii) return;

    if (ascii == '\n') {
        ts.input[ts.input_len] = '\0';
        hist_push(ts.input);
        ts.hist_idx = ts.hist_count;
        term_putchar('\n');
        term_process_command(ts.input);
        ts.input_len = 0;
        term_puts("> ");
        ts.prompt_col = ts.cur_col;
        window_set_dirty(term_win);
    } else if (ascii == '\b') {
        if (ts.input_len > 0) {
            ts.input_len--;
            if (ts.cur_col > 0) ts.cur_col--;
            ts.lines[ts.cur_row][ts.cur_col] = ' ';
            window_set_dirty(term_win);
        }
    } else if (ts.input_len < TERM_COLS - 4) {
        ts.input[ts.input_len++] = ascii;
        term_putchar(ascii);
        window_set_dirty(term_win);
    }
}

void app_terminal_open(void) {
    memset(&ts, 0, sizeof(ts));
    ts.total_lines = 1;

    term_win = window_create("Terminal", TERM_W, TERM_H);
    if (!term_win) return;

    term_win->on_draw  = term_draw;
    term_win->on_event = term_event;

    term_puts("MyOS Terminal v0.2\n");
    term_puts("Type 'help' for commands.\n");
    term_puts("> ");
    ts.prompt_col = ts.cur_col;

    wm_add_window(term_win);
}
