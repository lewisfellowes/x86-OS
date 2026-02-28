#include "mem/heap.h"
#include "mem/pmm.h"
#include "drivers/serial.h"
#include "lib/string.h"

#define HEAP_PAGES   64
#define HEADER_SIZE  8
#define HEAP_ALIGN   8
#define FLAG_USED    1
#define FLAG_FREE    0

typedef struct {
    uint32_t size;
    uint32_t flags;
} block_header_t;

static uint32_t heap_start;
static uint32_t heap_end;

void heap_init(void) {
    uint32_t first = pmm_alloc_frame();
    if (!first) { serial_puts("HEAP: alloc failed!\r\n"); return; }

    heap_start = first;
    for (int i = 1; i < HEAP_PAGES; i++) {
        uint32_t f = pmm_alloc_frame();
        if (!f) { serial_puts("HEAP: alloc failed!\r\n"); return; }
    }
    heap_end = heap_start + HEAP_PAGES * PAGE_SIZE;

    block_header_t *hdr = (block_header_t *)heap_start;
    hdr->size  = HEAP_PAGES * PAGE_SIZE - HEADER_SIZE;
    hdr->flags = FLAG_FREE;

    serial_puts("HEAP: 0x"); serial_hex32(heap_start);
    serial_puts("-0x");       serial_hex32(heap_end);
    serial_puts(" (");        serial_hex32((heap_end - heap_start) >> 10);
    serial_puts(" KiB)\r\n");
}

void *kmalloc(uint32_t size) {
    if (!size) return 0;
    size = (size + HEAP_ALIGN - 1) & ~(HEAP_ALIGN - 1);

    block_header_t *blk = (block_header_t *)heap_start;
    while ((uint32_t)blk < heap_end) {
        if (blk->flags == FLAG_FREE && blk->size >= size) {
            uint32_t remaining = blk->size - size;
            if (remaining > HEADER_SIZE + HEAP_ALIGN) {
                block_header_t *next = (block_header_t *)((uint8_t *)blk + HEADER_SIZE + size);
                next->size  = remaining - HEADER_SIZE;
                next->flags = FLAG_FREE;
                blk->size   = size;
            }
            blk->flags = FLAG_USED;
            return (void *)((uint8_t *)blk + HEADER_SIZE);
        }
        blk = (block_header_t *)((uint8_t *)blk + HEADER_SIZE + blk->size);
    }
    return 0;
}

void *kcalloc(uint32_t count, uint32_t size) {
    uint32_t total = count * size;
    void *p = kmalloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *blk = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    blk->flags = FLAG_FREE;

    /* Coalesce with next block */
    block_header_t *next = (block_header_t *)((uint8_t *)blk + HEADER_SIZE + blk->size);
    if ((uint32_t)next < heap_end && next->flags == FLAG_FREE)
        blk->size += HEADER_SIZE + next->size;
}
