#include "arch/boot_info.h"
#include "drivers/serial.h"
#include "arch/io.h"
#include "arch/gdt.h"
#include "arch/idt.h"
#include "arch/pic.h"
#include "arch/pit.h"
#include "drivers/kbd.h"
#include "mem/pmm.h"
#include "mem/paging.h"
#include "mem/heap.h"
#include "drivers/fb.h"
#include "gfx/cursor.h"
#include "drivers/mouse.h"
#include "drivers/ata.h"
#include "fs/fs.h"
#include "proc/process.h"
#include "proc/syscall.h"
#include "gui/event.h"
#include "gui/desktop.h"
#include "gui/wm.h"
#include "gui/compositor.h"

void kmain(boot_info_t *bi) {
    serial_init();
    serial_puts("kernel: C kernel starting\r\n");

    if (!bi || bi->magic != BOOTINFO_MAGIC) {
        serial_puts("FATAL: bad boot_info\r\n");
        for (;;) hlt();
    }
    serial_puts("boot_info OK, E820 entries=0x");
    serial_hex32(bi->memmap_len);
    serial_puts("\r\n");

    gdt_init();
    serial_puts("GDT loaded\r\n");

    pic_remap();
    idt_init();
    serial_puts("IDT + PIC ready\r\n");

    pit_init(100);
    kbd_init();

    pmm_init(bi);
    paging_init(pmm_total_count() << PAGE_SHIFT);
    heap_init();

    fb_init();
    ata_init();
    fs_init();
    process_init();
    syscall_init();
    mouse_init();

    event_init();
    compositor_init();
    desktop_init();
    desktop_draw();

    cursor_draw(mouse_get_state()->x, mouse_get_state()->y);
    fb_flip();
    sti();

    serial_puts("kernel: entering event loop\r\n");

    uint8_t prev_buttons = 0;

    for (;;) {
        hlt();

        int dirty = 0;

        /* Collect mouse events */
        mouse_state_t *ms = mouse_get_state();
        if (ms->updated) {
            mouse_clear_update();
            dirty = 1;

            event_t ev;
            ev.type    = EVENT_MOUSE_MOVE;
            ev.mouse.x = ms->x;
            ev.mouse.y = ms->y;
            ev.mouse.buttons = ms->buttons;
            event_push(&ev);

            uint8_t pressed  = ms->buttons & ~prev_buttons;
            uint8_t released = prev_buttons & ~ms->buttons;
            if (pressed & 1) {
                ev.type = EVENT_MOUSE_DOWN;
                event_push(&ev);
            }
            if (released & 1) {
                ev.type = EVENT_MOUSE_UP;
                event_push(&ev);
            }
            prev_buttons = ms->buttons;
        }

        /* Collect keyboard events */
        while (kbd_has_key()) {
            uint8_t sc = kbd_get_scancode();
            dirty = 1;
            event_t ev;
            if (sc & 0x80) {
                ev.type = EVENT_KEY_UP;
                ev.key.scancode = sc & 0x7F;
            } else {
                ev.type = EVENT_KEY_DOWN;
                ev.key.scancode = sc;
            }
            ev.key.ascii = kbd_scancode_to_ascii(ev.key.scancode);
            event_push(&ev);
        }

        /* Dispatch events — track if scene needs full redraw */
        int needs_scene_redraw = 0;
        event_t ev;
        while (event_poll(&ev)) {
            desktop_handle_event(&ev);
            if (ev.type != EVENT_MOUSE_MOVE)
                needs_scene_redraw = 1;
        }

        /* Clock tick may also dirty the screen */
        int clock_changed = desktop_update(pit_get_ticks(), 100);
        if (clock_changed)
            dirty = 1;

        /*
         * All drawing targets the invisible back buffer.
         * The user sees nothing until fb_flip().
         */
        cursor_erase();

        if (needs_scene_redraw) {
            compositor_redraw_all();
            dirty = 1;
        }

        wm_compose();
        cursor_draw(ms->x, ms->y);

        /* Single
         * memcpy to the visible LFB — flicker-free */
        if (dirty)
            fb_flip();
    }
}
