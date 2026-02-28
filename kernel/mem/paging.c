#include "mem/paging.h"
#include "mem/pmm.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define PTE_PRESENT  0x01
#define PTE_WRITABLE 0x02
#define PTE_FLAGS    (PTE_PRESENT | PTE_WRITABLE)

static uint32_t *page_directory;

static uint32_t *get_or_create_pt(uint32_t pd_index) {
    if (page_directory[pd_index] & PTE_PRESENT)
        return (uint32_t *)(page_directory[pd_index] & 0xFFFFF000);

    uint32_t pt_phys = pmm_alloc_frame();
    if (!pt_phys) return 0;
    memset((void *)pt_phys, 0, PAGE_SIZE);
    page_directory[pd_index] = pt_phys | PTE_FLAGS;
    return (uint32_t *)pt_phys;
}

void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    uint32_t *pt = get_or_create_pt(pd_idx);
    if (pt)
        pt[pt_idx] = (phys & 0xFFFFF000) | (flags & 0xFFF);
}

void paging_identity_map_range(uint32_t start, uint32_t size) {
    start &= 0xFFFFF000;
    for (uint32_t addr = start; addr < start + size; addr += PAGE_SIZE)
        paging_map_page(addr, addr, PTE_FLAGS);
}

void paging_init(uint32_t total_memory) {
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys) { serial_puts("PAGING: OOM!\r\n"); return; }

    page_directory = (uint32_t *)pd_phys;
    memset(page_directory, 0, PAGE_SIZE);

    /* Identity-map all usable memory in 4 MiB chunks */
    uint32_t num_pts = (total_memory + 0x3FFFFF) >> 22;
    for (uint32_t i = 0; i < num_pts; i++) {
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) { serial_puts("PAGING: OOM!\r\n"); return; }

        uint32_t *pt = (uint32_t *)pt_phys;
        uint32_t base = i << 22;
        for (uint32_t j = 0; j < 1024; j++)
            pt[j] = (base + j * PAGE_SIZE) | PTE_FLAGS;

        page_directory[i] = pt_phys | PTE_FLAGS;
    }

    __asm__ volatile ("mov %0, %%cr3" : : "r"(pd_phys));
    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
    __asm__ volatile ("jmp .paging_flush\n.paging_flush:");

    serial_puts("PAGING: enabled, PD=0x");
    serial_hex32(pd_phys);
    serial_puts(" PTs=0x");
    serial_hex32(num_pts);
    serial_puts("\r\n");
}

uint32_t paging_get_page_directory(void) {
    return (uint32_t)page_directory;
}
