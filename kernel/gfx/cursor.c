#include "gfx/cursor.h"
#include "drivers/fb.h"

static const uint8_t cursor_data[CURSOR_H][CURSOR_W] = {
    {1,0,0,0,0,0,0,0},
    {1,1,0,0,0,0,0,0},
    {1,2,1,0,0,0,0,0},
    {1,2,2,1,0,0,0,0},
    {1,2,2,2,1,0,0,0},
    {1,2,2,2,2,1,0,0},
    {1,2,2,2,2,2,1,0},
    {1,2,2,2,2,2,2,1},
    {1,2,2,2,1,1,0,0},
    {1,2,1,1,2,1,0,0},
    {1,1,0,0,1,2,1,0},
    {1,0,0,0,0,1,1,0},
};

static uint32_t save_buf[CURSOR_W * CURSOR_H];
static int      save_x, save_y;
static bool     drawn;

void cursor_draw(int x, int y) {
    save_x = x;
    save_y = y;

    for (int row = 0; row < CURSOR_H; row++) {
        for (int col = 0; col < CURSOR_W; col++) {
            save_buf[row * CURSOR_W + col] = fb_get_pixel(x + col, y + row);
            uint8_t v = cursor_data[row][col];
            if (v == 1)
                fb_set_pixel(x + col, y + row, 0x00000000);
            else if (v == 2)
                fb_set_pixel(x + col, y + row, 0x00FFFFFF);
        }
    }
    drawn = true;
}

void cursor_erase(void) {
    if (!drawn) return;
    for (int row = 0; row < CURSOR_H; row++)
        for (int col = 0; col < CURSOR_W; col++)
            fb_set_pixel(save_x + col, save_y + row,
                         save_buf[row * CURSOR_W + col]);
    drawn = false;
}

bool cursor_is_drawn(void) { return drawn; }
