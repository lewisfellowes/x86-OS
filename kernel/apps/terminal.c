#include "../gui/window.h"
#include "../gui/wm.h"
#include "../font.h"
#include "../fb.h"
#include "../string.h"
#include "../serial.h"
#include "../kbd.h"

#define TERM_COLS 48
#define TERM_ROWS 14
#define TERM_W (TERM_COLS * FONT_CHAR_WIDTH + 16)
#define TERM_H (TERM_ROWS * (FONT_CHAR_HEIGHT + 2) + 8)

typedef struct {
    char lines[TERM_ROWS][TERM_COLS + 1];
    int  cur_row;
    int  cur_col;
    char input[TERM_COLS + 1];
    int  input_len;
} term_state_t;

static term_state_t tstate;
static window_t *term_win;

static void term_scroll(void) {
    for (int i = 0; i < TERM_ROWS - 1; i++)
        memcpy(tstate.lines[i], tstate.lines[i + 1], TERM_COLS + 1);
    memset(tstate.lines[TERM_ROWS - 1], 0, TERM_COLS + 1);
    if (tstate.cur_row > 0)
        tstate.cur_row--;
}

static void term_putchar(char c) {
    if (c == '\n') {
        tstate.cur_col = 0;
        tstate.cur_row++;
        if (tstate.cur_row >= TERM_ROWS) term_scroll();
        return;
    }
    if (tstate.cur_col >= TERM_COLS) {
        tstate.cur_col = 0;
        tstate.cur_row++;
        if (tstate.cur_row >= TERM_ROWS) term_scroll();
    }
    tstate.lines[tstate.cur_row][tstate.cur_col++] = c;
}

static void term_puts(const char *s) {
    while (*s) term_putchar(*s++);
}

static void term_process_command(const char *cmd) {
    if (strcmp(cmd, "help") == 0) {
        term_puts("Commands: help, clear, uname, mem, echo <text>\n");
    } else if (strcmp(cmd, "clear") == 0) {
        memset(tstate.lines, 0, sizeof(tstate.lines));
        tstate.cur_row = 0;
        tstate.cur_col = 0;
    } else if (strcmp(cmd, "uname") == 0) {
        term_puts("MyOS 0.3 i686\n");
    } else if (strcmp(cmd, "mem") == 0) {
        term_puts("Memory info: see serial log\n");
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

    for (int r = 0; r < TERM_ROWS; r++) {
        int py = w->y + 4 + r * (FONT_CHAR_HEIGHT + 2);
        for (int c = 0; c < TERM_COLS; c++) {
            char ch = tstate.lines[r][c];
            if (ch)
                font_draw_char(w->x + 8 + c * FONT_CHAR_WIDTH, py,
                               ch, 0x0000FF00, 0x00181818);
        }
    }

    /* Draw prompt + input */
    int py = w->y + 4 + tstate.cur_row * (FONT_CHAR_HEIGHT + 2);
    int px = w->x + 8 + tstate.cur_col * FONT_CHAR_WIDTH;
    font_draw_string(px, py, "_ ", 0x0000FF00, 0x00181818);
}

static void term_event(window_t *w, int type, int d1, int d2) {
    (void)w; (void)d2;
    if (type != WIN_EVENT_KEY_DOWN) return;

    char ascii = kbd_scancode_to_ascii((uint8_t)d1);
    if (!ascii) return;

    if (ascii == '\n') {
        tstate.input[tstate.input_len] = '\0';
        term_putchar('\n');
        term_process_command(tstate.input);
        tstate.input_len = 0;
        term_puts("> ");
        window_set_dirty(term_win);
    } else if (ascii == '\b') {
        if (tstate.input_len > 0) {
            tstate.input_len--;
            if (tstate.cur_col > 0) tstate.cur_col--;
            tstate.lines[tstate.cur_row][tstate.cur_col] = ' ';
            window_set_dirty(term_win);
        }
    } else if (tstate.input_len < TERM_COLS - 2) {
        tstate.input[tstate.input_len++] = ascii;
        term_putchar(ascii);
        window_set_dirty(term_win);
    }
}

void app_terminal_open(void) {
    memset(&tstate, 0, sizeof(tstate));

    term_win = window_create("Terminal", TERM_W, TERM_H);
    if (!term_win) return;

    term_win->on_draw  = term_draw;
    term_win->on_event = term_event;

    term_puts("MyOS Terminal v0.1\n");
    term_puts("Type 'help' for commands.\n");
    term_puts("> ");

    wm_add_window(term_win);
}
