#pragma once
#include <stdint.h>
#include <stdbool.h>

#define CURSOR_W 8
#define CURSOR_H 12

void cursor_draw(int x, int y);
void cursor_erase(void);
bool cursor_is_drawn(void);
