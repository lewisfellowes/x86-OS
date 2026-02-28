#include "arch/idt.h"
#include "arch/pic.h"
#include "drivers/serial.h"
#include "arch/io.h"

#define IDT_ENTRIES 256

typedef struct {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idtr;
static irq_handler_t irq_handlers[16];

extern void idt_load(uint32_t idtr_ptr);

extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);
extern void isr32(void); extern void isr33(void); extern void isr34(void);
extern void isr35(void); extern void isr36(void); extern void isr37(void);
extern void isr38(void); extern void isr39(void); extern void isr40(void);
extern void isr41(void); extern void isr42(void); extern void isr43(void);
extern void isr44(void); extern void isr45(void); extern void isr46(void);
extern void isr47(void);
extern void isr48(void);

static irq_handler_t syscall_handler_fn;

static void idt_set_gate(uint8_t vec, uint32_t handler) {
    idt[vec].offset_low  = handler & 0xFFFF;
    idt[vec].selector    = 0x08;
    idt[vec].zero        = 0;
    idt[vec].type_attr   = 0x8E; /* present, ring 0, 32-bit interrupt gate */
    idt[vec].offset_high = (handler >> 16) & 0xFFFF;
}

static const char *exception_names[] = {
    "Divide Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "Bound Range", "Invalid Opcode", "No FPU",
    "Double Fault", "Coprocessor Overrun", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection", "Page Fault", "Reserved",
    "x87 FP Error", "Alignment Check", "Machine Check", "SIMD FP Error",
};

void isr_dispatch(isr_frame_t *frame) {
    if (frame->vector < 32) {
        serial_puts("\r\n*** EXCEPTION: ");
        if (frame->vector < 20)
            serial_puts(exception_names[frame->vector]);
        else
            serial_puts("Reserved");
        serial_puts(" (vec=0x");
        serial_hex8((uint8_t)frame->vector);
        serial_puts(" err=0x");
        serial_hex32(frame->error_code);
        serial_puts(" eip=0x");
        serial_hex32(frame->eip);
        serial_puts(")\r\n");
        cli();
        for (;;) hlt();
    }

    if (frame->vector == 48) {
        if (syscall_handler_fn)
            syscall_handler_fn(frame);
        return;
    }

    uint8_t irq = (uint8_t)(frame->vector - 32);
    if (irq < 16 && irq_handlers[irq])
        irq_handlers[irq](frame);
    pic_send_eoi(irq);
}

void irq_register(uint8_t irq, irq_handler_t handler) {
    if (irq < 16)
        irq_handlers[irq] = handler;
}

void syscall_register(irq_handler_t handler) {
    syscall_handler_fn = handler;
}

void idt_init(void) {
    idtr.limit = sizeof(idt) - 1;
    idtr.base  = (uint32_t)&idt;

    typedef void (*isr_stub_t)(void);
    isr_stub_t stubs[49] = {
        isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
        isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
        isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
        isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
        isr32, isr33, isr34, isr35, isr36, isr37, isr38, isr39,
        isr40, isr41, isr42, isr43, isr44, isr45, isr46, isr47,
        isr48,
    };

    for (int i = 0; i < 49; i++)
        idt_set_gate((uint8_t)i, (uint32_t)stubs[i]);

    idt_load((uint32_t)&idtr);
}
