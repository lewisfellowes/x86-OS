#pragma once
#include <stdint.h>

void     pit_init(uint32_t hz);
uint32_t pit_get_ticks(void);
void     sleep_ms(uint32_t ms);
