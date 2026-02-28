#include "arch/gdt.h"

typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

static gdt_entry_t gdt[5];
static gdt_ptr_t   gdtr;

extern void gdt_flush(uint32_t gdtr_ptr); /* in gdt_flush.asm */

static void gdt_set_entry(int i, uint32_t base, uint32_t limit,
                           uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void) {
    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base  = (uint32_t)&gdt;

    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF); /* ring 0 code */
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF); /* ring 0 data */
    gdt_set_entry(3, 0, 0xFFFFF, 0xFA, 0xCF); /* ring 3 code */
    gdt_set_entry(4, 0, 0xFFFFF, 0xF2, 0xCF); /* ring 3 data */

    gdt_flush((uint32_t)&gdtr);
}
