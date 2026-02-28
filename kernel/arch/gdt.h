#pragma once
#include <stdint.h>

#define GDT_CODE_SEL 0x08
#define GDT_DATA_SEL 0x10

void gdt_init(void);
