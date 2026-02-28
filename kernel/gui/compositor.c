#include "gui/compositor.h"
#include "gui/wm.h"
#include "gui/desktop.h"

void compositor_init(void) {
    /* placeholder for future dirty-rect tracking */
}

void compositor_redraw_all(void) {
    desktop_draw();
    wm_compose();
    desktop_draw_start_menu();
}
