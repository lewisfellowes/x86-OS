#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EVENT_NONE,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_DOWN,
    EVENT_MOUSE_UP,
    EVENT_KEY_DOWN,
    EVENT_KEY_UP,
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        struct { int32_t x, y; uint8_t buttons; } mouse;
        struct { uint8_t scancode; char ascii; }   key;
    };
} event_t;

void event_init(void);
void event_push(const event_t *ev);
bool event_poll(event_t *ev);
