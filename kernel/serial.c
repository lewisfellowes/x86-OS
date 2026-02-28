#include "serial.h"
#include "io.h"

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);   /* disable interrupts */
    outb(COM1_PORT + 3, 0x80);   /* enable DLAB */
    outb(COM1_PORT + 0, 0x01);   /* divisor lo = 1 (115200 baud) */
    outb(COM1_PORT + 1, 0x00);   /* divisor hi */
    outb(COM1_PORT + 3, 0x03);   /* 8N1, clear DLAB */
    outb(COM1_PORT + 2, 0xC7);   /* FIFO on, 14-byte threshold */
    outb(COM1_PORT + 4, 0x0B);   /* RTS/DSR set */
}

static void serial_wait_tx(void) {
    while (!(inb(COM1_PORT + 5) & 0x20))
        ;
}

void serial_putc(char c) {
    serial_wait_tx();
    outb(COM1_PORT, (uint8_t)c);
}

void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

static const char hex_chars[] = "0123456789ABCDEF";

void serial_hex8(uint8_t val) {
    serial_putc(hex_chars[(val >> 4) & 0xF]);
    serial_putc(hex_chars[val & 0xF]);
}

void serial_hex32(uint32_t val) {
    for (int i = 28; i >= 0; i -= 4)
        serial_putc(hex_chars[(val >> i) & 0xF]);
}
