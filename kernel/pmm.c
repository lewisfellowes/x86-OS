#include "pmm.h"
#include "serial.h"
#include "string.h"

extern uint8_t _kernel_end;

static uint8_t *bitmap;
static uint32_t bitmap_size;
static uint32_t total_frames;
static uint32_t free_frames;

static void set_frame(uint32_t frame) {
    bitmap[frame >> 3] |= (1 << (frame & 7));
}

static void clear_frame(uint32_t frame) {
    bitmap[frame >> 3] &= ~(1 << (frame & 7));
}

static int test_frame(uint32_t frame) {
    return bitmap[frame >> 3] & (1 << (frame & 7));
}

static void mark_region_free(uint32_t base, uint32_t len) {
    uint32_t start = (base + PAGE_SIZE - 1) >> PAGE_SHIFT;
    uint32_t end   = (base + len) >> PAGE_SHIFT;
    for (uint32_t f = start; f < end && f < total_frames; f++)
        clear_frame(f);
}

static void mark_region_used(uint32_t base, uint32_t len) {
    uint32_t start = base >> PAGE_SHIFT;
    uint32_t end   = (base + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
    for (uint32_t f = start; f < end && f < total_frames; f++)
        set_frame(f);
}

void pmm_init(boot_info_t *bi) {
    if (!bi || bi->memmap_len == 0) {
        serial_puts("PMM: no memory map!\r\n");
        return;
    }

    e820_entry_t *entries = (e820_entry_t *)(uint32_t)bi->memmap_ptr;
    uint32_t count = bi->memmap_len;

    uint32_t top = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type != E820_TYPE_USABLE) continue;
        if ((entries[i].base >> 32) != 0) continue;
        uint32_t end = (uint32_t)entries[i].base + (uint32_t)entries[i].length;
        if (end > top) top = end;
    }

    total_frames = top >> PAGE_SHIFT;
    bitmap_size  = (total_frames + 7) / 8;

    bitmap = (uint8_t *)(((uint32_t)&_kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));
    memset(bitmap, 0xFF, bitmap_size);

    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type != E820_TYPE_USABLE) continue;
        if ((entries[i].base >> 32) != 0) continue;
        mark_region_free((uint32_t)entries[i].base, (uint32_t)entries[i].length);
    }

    /* Reserve low 1MB + kernel + bitmap */
    mark_region_used(0, 0x100000);
    uint32_t kern_end = (uint32_t)bitmap + bitmap_size;
    mark_region_used(0x100000, kern_end - 0x100000);

    free_frames = 0;
    for (uint32_t i = 0; i < bitmap_size; i++) {
        uint8_t byte = ~bitmap[i];
        while (byte) { free_frames++; byte &= byte - 1; }
    }

    serial_puts("PMM: bitmap=0x");  serial_hex32((uint32_t)bitmap);
    serial_puts(" total=0x");       serial_hex32(total_frames);
    serial_puts(" free=0x");        serial_hex32(free_frames);
    serial_puts("\r\n");
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < bitmap_size; i++) {
        if (bitmap[i] == 0xFF) continue;
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (!(bitmap[i] & (1 << bit))) {
                uint32_t frame = i * 8 + bit;
                if (frame >= total_frames) return 0;
                set_frame(frame);
                free_frames--;
                return frame << PAGE_SHIFT;
            }
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr >> PAGE_SHIFT;
    if (frame < total_frames && test_frame(frame)) {
        clear_frame(frame);
        free_frames++;
    }
}

uint32_t pmm_free_count(void)  { return free_frames;  }
uint32_t pmm_total_count(void) { return total_frames; }
