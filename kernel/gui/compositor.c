#include "compositor.h"
#include "wm.h"
#include "desktop.h"

void compositor_init(void) {
    /* placeholder for future dirty-rect tracking */
}

void compositor_redraw_all(void) {
    desktop_draw();
    wm_compose();
}
