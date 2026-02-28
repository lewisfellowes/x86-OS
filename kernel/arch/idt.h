#pragma once
#include <stdint.h>

typedef struct {
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags;
} isr_frame_t;

typedef void (*irq_handler_t)(isr_frame_t *frame);

void idt_init(void);
void irq_register(uint8_t irq, irq_handler_t handler);
void syscall_register(irq_handler_t handler);
