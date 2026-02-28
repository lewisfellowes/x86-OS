#include "arch/pit.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/io.h"

#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43
#define PIT_BASE_HZ 1193182

static volatile uint32_t tick_count;
static uint32_t pit_freq;

static void pit_handler(isr_frame_t *frame) {
    (void)frame;
    tick_count++;
}

void pit_init(uint32_t hz) {
    pit_freq = hz;
    uint16_t divisor = (uint16_t)(PIT_BASE_HZ / hz);

    outb(PIT_COMMAND, 0x34); /* channel 0, lo/hi, mode 2, binary */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)(divisor >> 8));

    irq_register(0, pit_handler);
    pic_clear_mask(0);
}

uint32_t pit_get_ticks(void) {
    return tick_count;
}

void sleep_ms(uint32_t ms) {
    uint32_t target = tick_count + (ms * pit_freq) / 1000;
    while (tick_count < target)
        hlt();
}
