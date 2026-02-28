#pragma once
#include <stdint.h>
#include "boot_info.h"

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

void     pmm_init(boot_info_t *bi);
uint32_t pmm_alloc_frame(void);
void     pmm_free_frame(uint32_t addr);
uint32_t pmm_free_count(void);
uint32_t pmm_total_count(void);
