#pragma once
#include <stdint.h>

void paging_init(uint32_t total_memory);
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void paging_identity_map_range(uint32_t start, uint32_t size);
uint32_t paging_get_page_directory(void);
