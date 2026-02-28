#pragma once
#include <stdint.h>
#include <stdbool.h>

#define KBD_MOD_SHIFT 0x01
#define KBD_MOD_CTRL  0x02
#define KBD_MOD_ALT   0x04

void     kbd_init(void);
bool     kbd_has_key(void);
uint8_t  kbd_get_scancode(void);
char     kbd_scancode_to_ascii(uint8_t sc);
uint8_t  kbd_get_modifiers(void);
void     kbd_update_modifiers(uint8_t scancode, bool released);
