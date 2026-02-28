#include "../gui/window.h"
#include "../gui/wm.h"
#include "../gui/widget.h"
#include "../font.h"
#include "../fb.h"
#include "../string.h"

#define CALC_W 200
#define CALC_H 260

#define BTN_W 40
#define BTN_H 32
#define BTN_PAD 6
#define GRID_X 12
#define GRID_Y 48

typedef struct {
    int32_t  accumulator;
    int32_t  current;
    char     op;
    char     display[16];
    int      display_len;
    int      just_evaluated;
} calc_state_t;

static calc_state_t cs;
static window_t *calc_win;

static const char *btn_labels[4][4] = {
    { "7", "8", "9", "/" },
    { "4", "5", "6", "*" },
    { "1", "2", "3", "-" },
    { "C", "0", "=", "+" },
};

static void calc_update_display(void) {
    if (cs.display_len == 0) {
        cs.display[0] = '0';
        cs.display[1] = '\0';
    } else {
        cs.display[cs.display_len] = '\0';
    }
    window_set_dirty(calc_win);
}

static void calc_evaluate(void) {
    switch (cs.op) {
    case '+': cs.accumulator += cs.current; break;
    case '-': cs.accumulator -= cs.current; break;
    case '*': cs.accumulator *= cs.current; break;
    case '/':
        if (cs.current != 0) cs.accumulator /= cs.current;
        break;
    default:  cs.accumulator = cs.current; break;
    }

    /* Convert result to display string */
    int32_t val = cs.accumulator;
    int neg = 0;
    if (val < 0) { neg = 1; val = -val; }

    char tmp[16];
    int len = 0;
    if (val == 0) { tmp[len++] = '0'; }
    else while (val > 0 && len < 14) { tmp[len++] = '0' + (char)(val % 10); val /= 10; }
    if (neg) tmp[len++] = '-';

    cs.display_len = len;
    for (int i = 0; i < len; i++)
        cs.display[i] = tmp[len - 1 - i];
    cs.display[len] = '\0';

    cs.current = 0;
    cs.op = 0;
    cs.just_evaluated = 1;
}

static void calc_press(const char *lbl) {
    char c = lbl[0];

    if (c >= '0' && c <= '9') {
        if (cs.just_evaluated) {
            cs.display_len = 0;
            cs.just_evaluated = 0;
        }
        if (cs.display_len < 14) {
            cs.display[cs.display_len++] = c;
        }
        cs.current = 0;
        for (int i = 0; i < cs.display_len; i++)
            cs.current = cs.current * 10 + (cs.display[i] - '0');
    } else if (c == 'C') {
        cs.accumulator = 0;
        cs.current = 0;
        cs.op = 0;
        cs.display_len = 0;
        cs.just_evaluated = 0;
    } else if (c == '=') {
        calc_evaluate();
    } else if (c == '+' || c == '-' || c == '*' || c == '/') {
        if (cs.op && !cs.just_evaluated)
            calc_evaluate();
        else
            cs.accumulator = cs.current;
        cs.op = c;
        cs.display_len = 0;
        cs.just_evaluated = 0;
    }
    calc_update_display();
}

static void calc_draw(window_t *w) {
    fb_fill_rect(w->x, w->y, w->w, w->h, 0x00D0D0D0);

    /* Display area */
    fb_fill_rect(w->x + 10, w->y + 8, w->w - 20, 28, 0x00FFFFFF);
    fb_fill_rect(w->x + 10, w->y + 8, w->w - 20, 1, 0x00909090);
    fb_fill_rect(w->x + 10, w->y + 8, 1, 28, 0x00909090);
    int tw = font_string_width(cs.display);
    font_draw_string(w->x + w->w - 14 - tw, w->y + 14,
                     cs.display, 0x00000000, 0x00FFFFFF);

    /* Buttons */
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = w->x + GRID_X + c * (BTN_W + BTN_PAD);
            int by = w->y + GRID_Y + r * (BTN_H + BTN_PAD);
            uint32_t bg = 0x00E8E8E8;
            if (btn_labels[r][c][0] >= '0' && btn_labels[r][c][0] <= '9')
                bg = 0x00FFFFFF;
            else if (btn_labels[r][c][0] == '=')
                bg = 0x004080C0;
            else if (btn_labels[r][c][0] == 'C')
                bg = 0x00C06060;
            widget_draw_button(bx, by, BTN_W, BTN_H,
                               btn_labels[r][c], bg, 0x00202020);
        }
    }
}

static void calc_event(window_t *w, int type, int d1, int d2) {
    (void)w;
    if (type != WIN_EVENT_MOUSE_DOWN) return;

    int lx = d1;
    int ly = d2;

    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            int bx = GRID_X + c * (BTN_W + BTN_PAD);
            int by = GRID_Y + r * (BTN_H + BTN_PAD);
            if (lx >= bx && lx < bx + BTN_W &&
                ly >= by && ly < by + BTN_H) {
                calc_press(btn_labels[r][c]);
                return;
            }
        }
    }
}

void app_calc_open(void) {
    memset(&cs, 0, sizeof(cs));
    cs.display[0] = '0';
    cs.display[1] = '\0';

    calc_win = window_create("Calculator", CALC_W, CALC_H);
    if (!calc_win) return;

    calc_win->on_draw  = calc_draw;
    calc_win->on_event = calc_event;

    wm_add_window(calc_win);
}
