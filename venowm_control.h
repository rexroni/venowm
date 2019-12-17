#include <wayland-server.h>

#include "backend.h"

typedef struct {
    backend_t *be;
    struct wl_global *global;
} venowm_control_t;


void venowm_control_free(venowm_control_t *vc);

venowm_control_t *venowm_control_new(backend_t *be, struct wl_display *display);
