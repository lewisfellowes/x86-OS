#pragma once
#include <stdint.h>
#include <stddef.h>

void  heap_init(void);
void *kmalloc(uint32_t size);
void *kcalloc(uint32_t count, uint32_t size);
void  kfree(void *ptr);
