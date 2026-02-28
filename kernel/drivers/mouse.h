#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int32_t  x, y;
    uint8_t  buttons;
    bool     updated;
} mouse_state_t;

void mouse_init(void);
mouse_state_t *mouse_get_state(void);
void mouse_clear_update(void);
