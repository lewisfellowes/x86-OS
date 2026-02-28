#pragma once
#include <stdint.h>
#include <stdbool.h>

void    kbd_init(void);
bool    kbd_has_key(void);
uint8_t kbd_get_scancode(void);
char    kbd_scancode_to_ascii(uint8_t sc);
