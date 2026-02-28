#include "gui/widget.h"
#include "drivers/fb.h"
#include "gfx/font.h"

void widget_draw_button(int x, int y, int w, int h,
                        const char *label, uint32_t bg, uint32_t fg) {
    fb_fill_rect(x, y, w, h, bg);
    int tx = x + (w - font_string_width(label)) / 2;
    int ty = y + (h - FONT_CHAR_HEIGHT) / 2;
    font_draw_string(tx, ty, label, fg, bg);
}

void widget_draw_label(int x, int y, const char *text,
                       uint32_t fg, uint32_t bg) {
    font_draw_string(x, y, text, fg, bg);
}

bool widget_point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}
