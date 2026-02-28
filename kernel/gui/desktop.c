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
extern void app_editor_open(void);
extern void app_about_open(void);
extern void app_calc_open(void);

static uint32_t last_second;
static bool start_menu_open;

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

    uint32_t btn_bg = start_menu_open ? 0x00508848 : 0x00406838;
    widget_draw_button(4, FB_HEIGHT - 36, 64, 32, "Start", btn_bg, 0x00FFFFFF);

    fb_fill_rect(FB_WIDTH - 62, FB_HEIGHT - 34, 1, 24, 0x00506878);
}

/* ---------- start menu ---------- */

typedef struct {
    const char *label;
    void (*on_click)(void);
} menu_entry_t;

static const menu_entry_t menu_entries[] = {
    { "Terminal",    app_terminal_open },
    { "Files",       app_fileview_open },
    { "Editor",      app_editor_open },
    { "Calculator",  app_calc_open },
    { "About",       app_about_open },
};
#define NUM_MENU (int)(sizeof(menu_entries) / sizeof(menu_entries[0]))

#define MENU_X       4
#define MENU_W       130
#define MENU_ITEM_H  26
#define MENU_PAD     4
#define MENU_H       (NUM_MENU * MENU_ITEM_H + MENU_PAD * 2)
#define MENU_Y       (FB_HEIGHT - 40 - MENU_H)

void desktop_draw_start_menu(void) {
    if (!start_menu_open) return;

    /* Shadow */
    fb_fill_rect(MENU_X + 3, MENU_Y + 3, MENU_W, MENU_H, 0x00101820);

    /* Background */
    fb_fill_rect(MENU_X, MENU_Y, MENU_W, MENU_H, 0x002A3A4A);
    fb_fill_rect(MENU_X, MENU_Y, MENU_W, 1, 0x00506878);
    fb_fill_rect(MENU_X, MENU_Y, 1, MENU_H, 0x00506878);
    fb_fill_rect(MENU_X + MENU_W - 1, MENU_Y, 1, MENU_H, 0x00182830);

    for (int i = 0; i < NUM_MENU; i++) {
        int iy = MENU_Y + MENU_PAD + i * MENU_ITEM_H;
        font_draw_string(MENU_X + 12, iy + 5, menu_entries[i].label,
                         0x00E0E8F0, 0x002A3A4A);
        if (i < NUM_MENU - 1)
            fb_fill_rect(MENU_X + 8, iy + MENU_ITEM_H - 1,
                         MENU_W - 16, 1, 0x003A4A5A);
    }
}

static bool handle_start_menu_click(int mx, int my) {
    /* Check Start button */
    if (mx >= 4 && mx < 68 && my >= FB_HEIGHT - 36 && my < FB_HEIGHT - 4) {
        start_menu_open = !start_menu_open;
        return true;
    }

    if (!start_menu_open) return false;

    /* Check menu items */
    if (mx >= MENU_X && mx < MENU_X + MENU_W &&
        my >= MENU_Y && my < MENU_Y + MENU_H) {
        int idx = (my - MENU_Y - MENU_PAD) / MENU_ITEM_H;
        if (idx >= 0 && idx < NUM_MENU) {
            menu_entries[idx].on_click();
            start_menu_open = false;
            return true;
        }
    }

    /* Click outside menu closes it */
    start_menu_open = false;
    return false;
}

/* ---------- desktop icons ---------- */

typedef struct {
    const char *label;
    int x, y;
    uint32_t icon_color;
    void (*on_click)(void);
} desktop_icon_t;

static const desktop_icon_t icons[] = {
    { "Terminal",  16, 20,  0x00406888, app_terminal_open },
    { "Files",     16, 105, 0x00D8B838, app_fileview_open },
    { "Editor",    16, 190, 0x00E07050, app_editor_open },
    { "Calc",      16, 275, 0x009060A0, app_calc_open },
    { "About",     16, 360, 0x00508890, app_about_open },
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
    start_menu_open = false;
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
    if (ev->type == EVENT_KEY_DOWN && ev->key.scancode == 0x01 && start_menu_open) {
        start_menu_open = false;
        return;
    }

    if (ev->type == EVENT_MOUSE_DOWN) {
        if (handle_start_menu_click(ev->mouse.x, ev->mouse.y))
            return;
    }

    if (ev->type == EVENT_MOUSE_DOWN || ev->type == EVENT_MOUSE_UP ||
        ev->type == EVENT_MOUSE_MOVE ||
        ev->type == EVENT_KEY_DOWN || ev->type == EVENT_KEY_UP) {
        bool consumed = wm_handle_event(ev);
        if (ev->type == EVENT_MOUSE_DOWN && !consumed)
            handle_icon_click(ev->mouse.x, ev->mouse.y);
    }
}

bool desktop_start_menu_visible(void) {
    return start_menu_open;
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
