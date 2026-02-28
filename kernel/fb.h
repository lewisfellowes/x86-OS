#pragma once
#include <stdint.h>
#include <stdbool.h>

#define FB_WIDTH  640
#define FB_HEIGHT 480
#define FB_BPP    32
#define FB_PITCH  (FB_WIDTH * 4)

void     fb_init(void);
uint32_t fb_get_addr(void);
void     fb_fill_rect(int x, int y, int w, int h, uint32_t color);
void     fb_set_pixel(int x, int y, uint32_t color);
uint32_t fb_get_pixel(int x, int y);
