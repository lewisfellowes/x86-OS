#pragma once
#include <stdint.h>

#define COM1_PORT 0x3F8

void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_hex32(uint32_t val);
void serial_hex8(uint8_t val);
