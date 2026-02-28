#include "gui/clock.h"
#include "drivers/fb.h"
#include "gfx/font.h"

void clock_draw(uint32_t ticks, uint32_t hz) {
    uint32_t total_secs = ticks / hz;
    uint32_t mins = (total_secs / 60) % 100;
    uint32_t secs = total_secs % 60;

    char buf[6];
    buf[0] = '0' + (char)(mins / 10);
    buf[1] = '0' + (char)(mins % 10);
    buf[2] = ':';
    buf[3] = '0' + (char)(secs / 10);
    buf[4] = '0' + (char)(secs % 10);
    buf[5] = '\0';

    fb_fill_rect(FB_WIDTH - 56, FB_HEIGHT - 34, 48, 24, 0x00283040);
    font_draw_string(FB_WIDTH - 48, FB_HEIGHT - 28, buf, 0x00C0D0E0, 0x00283040);
}
