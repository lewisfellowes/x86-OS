#pragma once
#include <stdint.h>
#include <stdbool.h>

void widget_draw_button(int x, int y, int w, int h,
                        const char *label, uint32_t bg, uint32_t fg);
void widget_draw_label(int x, int y, const char *text,
                       uint32_t fg, uint32_t bg);
bool widget_point_in_rect(int px, int py, int rx, int ry, int rw, int rh);
