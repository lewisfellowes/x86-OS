#include "mouse.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "serial.h"
#include "fb.h"

static mouse_state_t state;
static uint8_t cycle;
static uint8_t packet[3];

static void mouse_wait_write(void) {
    for (int i = 0; i < 100000; i++)
        if (!(inb(0x64) & 2)) return;
}

static void mouse_wait_read(void) {
    for (int i = 0; i < 100000; i++)
        if (inb(0x64) & 1) return;
}

static void mouse_cmd(uint8_t cmd) {
    mouse_wait_write(); outb(0x64, 0xD4);
    mouse_wait_write(); outb(0x60, cmd);
    mouse_wait_read();  inb(0x60);
}

static void mouse_handler(isr_frame_t *frame) {
    (void)frame;
    uint8_t status = inb(0x64);
    uint8_t data   = inb(0x60);

    if (!(status & 0x20)) return;

    if (cycle == 0 && !(data & 0x08)) return;

    packet[cycle++] = data;
    if (cycle < 3) return;
    cycle = 0;

    if (packet[0] & 0xC0) return;

    state.buttons = packet[0] & 0x07;

    int32_t dx = (int32_t)packet[1];
    if (packet[0] & 0x10) dx |= (int32_t)0xFFFFFF00;
    state.x += dx;

    int32_t dy = (int32_t)packet[2];
    if (packet[0] & 0x20) dy |= (int32_t)0xFFFFFF00;
    state.y -= dy;

    if (state.x < 0) state.x = 0;
    if (state.y < 0) state.y = 0;
    if (state.x > FB_WIDTH  - 8) state.x = FB_WIDTH  - 8;
    if (state.y > FB_HEIGHT - 12) state.y = FB_HEIGHT - 12;

    state.updated = true;
}

void mouse_init(void) {
    while (inb(0x64) & 1) inb(0x60);

    mouse_wait_write(); outb(0x64, 0xA8);

    mouse_wait_write(); outb(0x64, 0x20);
    mouse_wait_read();
    uint8_t cfg = inb(0x60) | 0x02;
    mouse_wait_write(); outb(0x64, 0x60);
    mouse_wait_write(); outb(0x60, cfg);

    mouse_cmd(0xF6);
    mouse_cmd(0xF4);

    while (inb(0x64) & 1) inb(0x60);

    state.x = FB_WIDTH / 2;
    state.y = FB_HEIGHT / 2;
    cycle = 0;
    state.updated = false;

    irq_register(12, mouse_handler);
    pic_clear_mask(12);

    serial_puts("mouse: PS/2 init OK\r\n");
}

mouse_state_t *mouse_get_state(void) { return &state; }
void mouse_clear_update(void) { state.updated = false; }
