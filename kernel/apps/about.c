#include "gui/window.h"
#include "gui/wm.h"
#include "gfx/font.h"
#include "drivers/fb.h"
#include "mem/pmm.h"
#include "arch/pit.h"

#define ABOUT_W 300
#define ABOUT_H 200

static void about_draw(window_t *w) {
    fb_fill_rect(w->x, w->y, w->w, w->h, 0x00F0F0F0);

    int y = w->y + 10;
    int x = w->x + 16;
    uint32_t fg = 0x00202020;
    uint32_t bg = 0x00F0F0F0;
    uint32_t hd = 0x002050A0;

    font_draw_string(x, y, "My OS v0.3", hd, bg);
    y += 24;
    font_draw_string(x, y, "32-bit x86 Operating System", fg, bg);
    y += 20;
    font_draw_string(x, y, "NASM + i686-elf-gcc", fg, bg);
    y += 20;
    font_draw_string(x, y, "Bochs VGA 640x480x32bpp", fg, bg);
    y += 28;

    /* Dynamic info */
    uint32_t free_mb = (pmm_free_count() * 4) / 1024;
    uint32_t total_mb = (pmm_total_count() * 4) / 1024;
    char ram_str[40];
    ram_str[0] = 'R'; ram_str[1] = 'A'; ram_str[2] = 'M';
    ram_str[3] = ':'; ram_str[4] = ' ';
    /* Simple number formatting */
    int pos = 5;
    if (free_mb >= 10) ram_str[pos++] = '0' + (char)(free_mb / 10);
    ram_str[pos++] = '0' + (char)(free_mb % 10);
    ram_str[pos++] = '/';
    if (total_mb >= 100) ram_str[pos++] = '0' + (char)(total_mb / 100);
    if (total_mb >= 10) ram_str[pos++] = '0' + (char)((total_mb / 10) % 10);
    ram_str[pos++] = '0' + (char)(total_mb % 10);
    ram_str[pos++] = ' '; ram_str[pos++] = 'M'; ram_str[pos++] = 'B';
    ram_str[pos] = '\0';
    font_draw_string(x, y, ram_str, fg, bg);
    y += 20;

    uint32_t secs = pit_get_ticks() / 100;
    char up_str[24] = "Uptime: ";
    pos = 8;
    uint32_t mins = secs / 60;
    secs = secs % 60;
    if (mins >= 10) up_str[pos++] = '0' + (char)(mins / 10);
    up_str[pos++] = '0' + (char)(mins % 10);
    up_str[pos++] = ':';
    up_str[pos++] = '0' + (char)(secs / 10);
    up_str[pos++] = '0' + (char)(secs % 10);
    up_str[pos] = '\0';
    font_draw_string(x, y, up_str, fg, bg);
}

void app_about_open(void) {
    window_t *w = window_create("About My OS", ABOUT_W, ABOUT_H);
    if (!w) return;
    w->on_draw = about_draw;
    wm_add_window(w);
}
