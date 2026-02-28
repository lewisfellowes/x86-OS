#include "gui/desktop.h"
#include "gui/clock.h"
#include "gui/widget.h"
#include "gui/wm.h"
#include "drivers/fb.h"
#include "gfx/font.h"

#include <stddef.h>
#include <stdint.h>

/* forward declarations for app launchers */
extern void app_terminal_open(void);
extern void app_fileview_open(void);
extern void app_about_open(void);
extern void app_calc_open(void);

static uint32_t last_second;

/* ---------- gradient ---------- */

static void draw_gradient(void) {
    uint32_t *fb = (uint32_t *)(uintptr_t)fb_get_addr();
    if (!fb) return;

    for (int band = 0; band < 22; band++) {
        int r = 0x44 - 0x30 * band / 21;
        int g = 0x78 - 0x50 * band / 21;
        int b = 0xB8 - 0x70 * band / 21;
        uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        for (int row = band * 20; row < (band + 1) * 20; row++)
            for (int col = 0; col < FB_WIDTH; col++)
                fb[row * FB_WIDTH + col] = color;
    }
}

/* ---------- taskbar ---------- */

static void draw_taskbar(void) {
    fb_fill_rect(0, FB_HEIGHT - 40, FB_WIDTH, 2, 0x00506070);
    fb_fill_rect(0, FB_HEIGHT - 38, FB_WIDTH, 38, 0x00384858);
    widget_draw_button(4, FB_HEIGHT - 36, 64, 32, "Start", 0x00406838, 0x00FFFFFF);
    fb_fill_rect(FB_WIDTH - 62, FB_HEIGHT - 34, 1, 24, 0x00506878);
}

/* ---------- desktop icons ---------- */

typedef struct {
    const char *label;
    int x, y;
    uint32_t icon_color;
    void (*on_click)(void);
} desktop_icon_t;

static const desktop_icon_t icons[] = {
    { "Terminal",  16, 60,  0x00406888, app_terminal_open },
    { "Files",     16, 160, 0x00D8B838, app_fileview_open },
    { "About",     16, 260, 0x00508890, app_about_open },
    { "Calc",      16, 360, 0x009060A0, app_calc_open },
};
#define NUM_ICONS (int)(sizeof(icons) / sizeof(icons[0]))

static void draw_icons(void) {
    for (int i = 0; i < NUM_ICONS; i++) {
        const desktop_icon_t *ic = &icons[i];
        fb_fill_rect(ic->x + 14, ic->y, 42, 34, ic->icon_color);
        font_draw_string(ic->x + 1, ic->y + 39, ic->label, 0x00102030, FB_TRANSPARENT);
        font_draw_string(ic->x, ic->y + 38, ic->label, 0x00FFFFFF, FB_TRANSPARENT);
    }
}

static void handle_icon_click(int mx, int my) {
    for (int i = 0; i < NUM_ICONS; i++) {
        const desktop_icon_t *ic = &icons[i];
        if (mx >= ic->x && mx < ic->x + 70 &&
            my >= ic->y && my < ic->y + 54) {
            if (ic->on_click)
                ic->on_click();
            return;
        }
    }
}

/* ---------- public interface ---------- */

void desktop_init(void) {
    last_second = 0;
    wm_init();
}

void desktop_draw_background(void) {
    draw_gradient();
    draw_taskbar();
    draw_icons();
}

void desktop_draw(void) {
    desktop_draw_background();
}

void desktop_handle_event(const event_t *ev) {
    /* Let WM handle first (it consumes events for windows) */
    if (ev->type == EVENT_MOUSE_DOWN || ev->type == EVENT_MOUSE_UP ||
        ev->type == EVENT_MOUSE_MOVE ||
        ev->type == EVENT_KEY_DOWN || ev->type == EVENT_KEY_UP) {
        wm_handle_event(ev);
    }

    /* Desktop icon clicks (only if no window was hit) */
    if (ev->type == EVENT_MOUSE_DOWN) {
        handle_icon_click(ev->mouse.x, ev->mouse.y);
    }
}

int desktop_update(uint32_t ticks, uint32_t hz) {
    uint32_t sec = ticks / hz;
    if (sec != last_second) {
        last_second = sec;
        clock_draw(ticks, hz);
        return 1;
    }
    return 0;
}
