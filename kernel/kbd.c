#include "kbd.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

#define KBD_BUF_SIZE 64

static volatile uint8_t buf[KBD_BUF_SIZE];
static volatile uint8_t buf_head;
static volatile uint8_t buf_tail;

static const char scancode_table[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};

static void kbd_handler(isr_frame_t *frame) {
    (void)frame;
    uint8_t sc = inb(0x60);
    uint8_t next = (buf_head + 1) % KBD_BUF_SIZE;
    if (next != buf_tail) {
        buf[buf_head] = sc;
        buf_head = next;
    }
}

void kbd_init(void) {
    buf_head = 0;
    buf_tail = 0;
    irq_register(1, kbd_handler);
    pic_clear_mask(1);
}

bool kbd_has_key(void) {
    return buf_head != buf_tail;
}

uint8_t kbd_get_scancode(void) {
    while (!kbd_has_key())
        hlt();
    uint8_t sc = buf[buf_tail];
    buf_tail = (buf_tail + 1) % KBD_BUF_SIZE;
    return sc;
}

char kbd_scancode_to_ascii(uint8_t sc) {
    if (sc & 0x80) return 0;
    if (sc >= 128) return 0;
    return scancode_table[sc];
}
