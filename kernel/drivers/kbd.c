#include "drivers/kbd.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/io.h"

#define KBD_BUF_SIZE 64

static volatile uint8_t buf[KBD_BUF_SIZE];
static volatile uint8_t buf_head;
static volatile uint8_t buf_tail;
static uint8_t modifiers;

static const char scancode_lower[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ',
};

static const char scancode_upper[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?', 0,
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
    modifiers = 0;
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

void kbd_update_modifiers(uint8_t scancode, bool released) {
    uint8_t bit = 0;
    switch (scancode) {
    case 0x2A: case 0x36: bit = KBD_MOD_SHIFT; break;
    case 0x1D:            bit = KBD_MOD_CTRL;  break;
    case 0x38:            bit = KBD_MOD_ALT;   break;
    default: return;
    }
    if (released)
        modifiers &= ~bit;
    else
        modifiers |= bit;
}

uint8_t kbd_get_modifiers(void) {
    return modifiers;
}

char kbd_scancode_to_ascii(uint8_t sc) {
    if (sc & 0x80) return 0;
    if (sc >= 128) return 0;
    if (modifiers & KBD_MOD_SHIFT)
        return scancode_upper[sc];
    return scancode_lower[sc];
}
