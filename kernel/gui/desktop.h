#pragma once
#include <stdint.h>
#include "gui/event.h"

void desktop_init(void);
void desktop_draw(void);
void desktop_draw_background(void);
void desktop_handle_event(const event_t *ev);
int  desktop_update(uint32_t ticks, uint32_t hz);
