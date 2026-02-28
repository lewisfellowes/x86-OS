#pragma once
#include "window.h"
#include "event.h"

void wm_init(void);
void wm_add_window(window_t *win);
void wm_remove_window(window_t *win);
void wm_focus(window_t *win);
void wm_handle_event(const event_t *ev);
void wm_compose(void);
window_t *wm_get_window(int index);
int       wm_window_count(void);
