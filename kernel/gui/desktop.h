#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "gui/event.h"

void desktop_init(void);
void desktop_draw(void);
void desktop_draw_background(void);
void desktop_draw_start_menu(void);
void desktop_handle_event(const event_t *ev);
bool desktop_start_menu_visible(void);
int  desktop_update(uint32_t ticks, uint32_t hz);
