#include "font.h"
#include "fb.h"
#include "string.h"

#define VGA_FONT_ADDR 0x4000

void font_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = (const uint8_t *)VGA_FONT_ADDR + (uint8_t)c * 16;
    volatile uint32_t *fb = (volatile uint32_t *)fb_get_addr();
    if (!fb) return;

    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= FB_HEIGHT) continue;
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            int sx = x + col;
            if (sx < 0 || sx >= FB_WIDTH) continue;
            if (bits & 0x80) {
                fb[sy * FB_WIDTH + sx] = fg;
            } else if (!(bg & 0xFF000000)) {
                fb[sy * FB_WIDTH + sx] = bg;
            }
            bits <<= 1;
        }
    }
}

void font_draw_string(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    while (*s) {
        font_draw_char(x, y, *s, fg, bg);
        x += FONT_CHAR_WIDTH;
        s++;
    }
}

int font_string_width(const char *s) {
    return (int)strlen(s) * FONT_CHAR_WIDTH;
}
