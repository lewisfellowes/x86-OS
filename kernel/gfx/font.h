#pragma once
#include <stdint.h>

#define FONT_CHAR_WIDTH  8
#define FONT_CHAR_HEIGHT 16

#define FB_TRANSPARENT 0xFF000000

void font_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void font_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg);
int  font_string_width(const char *s);
