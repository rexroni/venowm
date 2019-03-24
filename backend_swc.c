// for setenv:
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>

#include <swc.h>

#include "backend.h"
#include "logmsg.h"

// types
struct backend_t {
    struct wl_display *disp;
};


// screen handlers:

static void backend_handle_screen_destroy(void *data){
    handle_screen_destroy(data);
}

static void backend_handle_screen_geometry(void *data){
    handle_screen_geometry(data);
}

static struct swc_screen_handler screen_handler = {
    .destroy = backend_handle_screen_destroy,
    .geometry_changed = backend_handle_screen_geometry,
    .usable_geometry_changed = backend_handle_screen_geometry,
    .entered = NULL,
};

static void backend_handle_screen_new(struct swc_screen *swc_screen){
    void *cb_data;
    if(handle_screen_new(swc_screen, &cb_data)){
        // error in handle_screen_new, don't set any callbacks
        return;
    }

    swc_screen_set_handler(swc_screen, &screen_handler, cb_data);
}


// window handlers:

static void backend_handle_window_destroy(void *data){
    handle_window_destroy(data);
}

static struct swc_window_handler window_handler = {
    .destroy = &backend_handle_window_destroy,
    .title_changed = NULL,
    .app_id_changed = NULL,
    .parent_changed = NULL,
    .entered = NULL,
    .move = NULL,
    .resize = NULL,
};

static void backend_handle_window_new(struct swc_window *swc_window){
    void *cb_data;

    // set some defaults
    swc_window_set_border(swc_window, 0, 0);
    swc_window_set_tiled(swc_window);

    if(handle_window_new(swc_window, &cb_data)){
        // error in handle_window_new, don't set any callbacks
        return;
    }

    swc_window_set_handler(swc_window, &window_handler, cb_data);
}


// swc manager

static struct swc_manager manager = {.new_screen=&backend_handle_screen_new,
                                     .new_window=&backend_handle_window_new};

// primary backend functions

backend_t *backend_new(void){
    backend_t *be = malloc(sizeof(*be));
    if(!be){
        perror("malloc backend");
        return NULL;
    }
    *be = (backend_t){0};

    be->disp = wl_display_create();
    if(be->disp == NULL){
        logmsg("failed in wl_display_create()\n");
        goto cu_backend;
    }

    const char *wl_sock = wl_display_add_socket_auto(be->disp);
    if(wl_sock == NULL){
        logmsg("failed in wl_display_create()\n");
        goto cu_display;
    }

    if(setenv("WAYLAND_DISPLAY", wl_sock, 0)){
        perror("set environment");
        goto cu_display;
    }

    if(!swc_initialize(be->disp, NULL, &manager)){
        logmsg("failed in swc_initialize()\n");
        goto cu_display;
    }

    return be;

cu_display:
    wl_display_destroy(be->disp);
cu_backend:
    free(be);
    return NULL;
}

void backend_free(backend_t *be){
    swc_finalize();
    wl_display_destroy(be->disp);
    free(be);
}

int backend_run(backend_t *be){
    // // not sure what this variable did in the example, seems useless:
    // struct wl_event_loop *event_loop = wl_display_get_event_loop(disp);
    // if(event_loop == NULL){
    //     retval = 7;
    //     goto cu_swc;
    // }

    wl_display_run(be->disp);

    return 0;
}

void backend_stop(backend_t *be){
    wl_display_terminate(be->disp);
}

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
                  be_key_handler_t handler, void *data){
    (void)be;
    return swc_add_binding(SWC_BINDING_KEY, mods, key, handler, data);
}

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h){
    *x = be_screen->usable_geometry.x;
    *y = be_screen->usable_geometry.y;
    *w = be_screen->usable_geometry.width;
    *h = be_screen->usable_geometry.height;
}

void be_window_focus(be_window_t *be_window){
    swc_window_focus(be_window);
}

void be_window_hide(be_window_t *be_window){
    swc_window_hide(be_window);
}

void be_window_show(be_window_t *be_window){
    swc_window_show(be_window);
}

void be_window_close(be_window_t *be_window){
    swc_window_close(be_window);
}

void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h){
    swc_window_set_position(be_window, x, y);
    swc_window_set_size(be_window, w, h);
}
