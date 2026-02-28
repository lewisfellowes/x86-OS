#include "drivers/fb.h"
#include "arch/io.h"
#include "mem/paging.h"
#include "mem/pmm.h"
#include "drivers/serial.h"

#include <stdint.h>
#include <stddef.h>

#define BGA_INDEX_PORT 0x01CE
#define BGA_DATA_PORT  0x01CF
#define BGA_REG_ID     0
#define BGA_REG_XRES   1
#define BGA_REG_YRES   2
#define BGA_REG_BPP    3
#define BGA_REG_ENABLE 4
#define BGA_FLAG_ENABLED 0x01
#define BGA_FLAG_LFB     0x40

#define PCI_ADDR_PORT  0x0CF8
#define PCI_DATA_PORT  0x0CFC

static uint32_t lfb_addr;
static uint32_t backbuf[FB_WIDTH * FB_HEIGHT];

static void bga_write(uint16_t reg, uint16_t val) {
    outw(BGA_INDEX_PORT, reg);
    outw(BGA_DATA_PORT, val);
}

static uint16_t bga_read(uint16_t reg) {
    outw(BGA_INDEX_PORT, reg);
    return inw(BGA_DATA_PORT);
}

static void fb_map_4mb(uint32_t addr) {
    addr &= 0xFFC00000;
    uint32_t pt_phys = pmm_alloc_frame();
    if (!pt_phys) return;

    uint32_t *pt = (uint32_t *)(uintptr_t)pt_phys;
    for (uint32_t i = 0; i < 1024; i++)
        pt[i] = (addr + i * PAGE_SIZE) | 0x03;

    uint32_t *pd = (uint32_t *)(uintptr_t)paging_get_page_directory();
    pd[addr >> 22] = pt_phys | 0x03;

    uintptr_t cr3;
    __asm__ volatile ("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile ("mov %0, %%cr3" : : "r"(cr3));
}

void fb_init(void) {
    uint16_t id = bga_read(BGA_REG_ID);
    if (id < 0xB0C0) {
        serial_puts("FB: BGA not detected\r\n");
        return;
    }

    outl(PCI_ADDR_PORT, 0x80001010);
    lfb_addr = inl(PCI_DATA_PORT) & 0xFFFFFFF0;

    bga_write(BGA_REG_ENABLE, 0);
    bga_write(BGA_REG_XRES, FB_WIDTH);
    bga_write(BGA_REG_YRES, FB_HEIGHT);
    bga_write(BGA_REG_BPP, FB_BPP);
    bga_write(BGA_REG_ENABLE, BGA_FLAG_ENABLED | BGA_FLAG_LFB);

    fb_map_4mb(lfb_addr);

    serial_puts("FB: LFB=0x"); serial_hex32(lfb_addr);
    serial_puts(" 640x480x32bpp (double-buffered)\r\n");
}

uint32_t fb_get_addr(void) { return (uintptr_t)backbuf; }

void fb_set_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    backbuf[y * FB_WIDTH + x] = color;
}

uint32_t fb_get_pixel(int x, int y) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return 0;
    return backbuf[y * FB_WIDTH + x];
}

void fb_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!lfb_addr) return;
    for (int row = y; row < y + h && row < FB_HEIGHT; row++) {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < FB_WIDTH; col++) {
            if (col < 0) continue;
            backbuf[row * FB_WIDTH + col] = color;
        }
    }
}

void fb_flip(void) {
    if (!lfb_addr) return;
    const uint32_t *src = backbuf;
    volatile uint32_t *dst = (volatile uint32_t *)(uintptr_t)lfb_addr;
    uint32_t n = FB_WIDTH * FB_HEIGHT;
    for (uint32_t i = 0; i < n; i++)
        dst[i] = src[i];
}
