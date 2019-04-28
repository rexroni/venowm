#include <stdlib.h>
#include <wayland-server.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/interface.h>

#include "backend.h"
#include "venowm.h"
#include "logmsg.h"

static void exec(const char *shcmd){
    logmsg("called exec\n");
    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        return;
    }
    if(pid == 0){
        // child
        execl("/bin/sh", "/bin/sh", "-c", shcmd, NULL);
        perror("execl");
        exit(127);
    }
    // parent continues with whatever it was doing
    return;
}

struct be_screen_t {
    // cb_data is set by the frontend, we don't touch it.
    void *cb_data;
    backend_t *be;
    struct wlr_output *output;
    struct wl_list link; // backend_t.outputs
    struct wl_listener output_destroyed_listener;
    struct wl_listener frame_listener;
};

struct be_window_t {
};

struct backend_t {
    struct wl_display *display;
    struct wl_event_loop *loop;
    // the wayland backend
    struct wlr_backend *wlr_backend;
    // the wlr compositor
    struct wlr_compositor *compositor;
    // shell interfaces
    struct wlr_xdg_shell_v6 *xdg_shell_v6;
    // outputs
    struct wl_listener new_output_listener;
    struct wl_list be_screens;
    const char *socket;
};

///// Backend Screen Functions

static void be_screen_free(be_screen_t *be_screen){
    // call venowm's screen_destroy handler
    handle_screen_destroy(be_screen->cb_data);
    wl_list_remove(&be_screen->frame_listener.link);
    wl_list_remove(&be_screen->output_destroyed_listener.link);
    wl_list_remove(&be_screen->link);
    // TODO: free view
    // TODO: free surface
    free(be_screen);
}

static void handle_output_destroyed(struct wl_listener *l, void *data){
    (void)data;
    be_screen_t *be_screen = wl_container_of(l, be_screen,
                                             output_destroyed_listener);
    be_screen_free(be_screen);
}

static void handle_frame(struct wl_listener *l, void *data){
    (void)data;
    be_screen_t *be_screen = wl_container_of(l, be_screen,
                                             output_destroyed_listener);

    // wait, why the fuck are these two lines not the same???
    //struct wlr_output *o = be_screen->output; // segfaults
    struct wlr_output *o = data; // works

    struct wlr_renderer *r = wlr_backend_get_renderer(o->backend);

    // prepare the output for rendering
    int age = -1;
    wlr_output_make_current(o, &age);
    wlr_renderer_begin(r, o->width, o->height);

    // render a blue square
    float color[4] = {0.0, 0.0, 0.5, 1.0};
    wlr_renderer_clear(r, color);

    // done rendering, commit buffer
    wlr_output_swap_buffers(o, NULL, NULL);
    wlr_renderer_end(r);
}

static be_screen_t *be_screen_new(backend_t *be, struct wlr_output *output){
    be_screen_t *be_screen = malloc(sizeof(*be_screen));
    if(!be_screen) return NULL;
    *be_screen = (be_screen_t){0};

    be_screen->be = be;
    be_screen->output = output;

    // set mode, for backends with modes (the last mode is typically best)
    if(!wl_list_empty(&output->modes)){
        struct wlr_output_mode *mode;
        mode = wl_container_of(output->modes.prev, mode, link);
        wlr_output_set_mode(output, mode);
    }

    // add a blank background surface
    // TODO: add a blank background

    // add a view to the background
    // TODO: add a view to blank background

    wl_list_insert(be->be_screens.prev, &be_screen->link);

    be_screen->output_destroyed_listener.notify = handle_output_destroyed;
    wl_signal_add(&output->events.destroy,
                  &be_screen->output_destroyed_listener);

    be_screen->frame_listener.notify = handle_frame;
    wl_signal_add(&output->events.frame, &be_screen->frame_listener);

    // TODO: handle resize/move events?

    // call venowm's new screen handler and get cb_data
    if(handle_screen_new(be_screen, &be_screen->cb_data)){
        goto cu_listeners;
    }

    return be_screen;

cu_listeners:
    wl_list_remove(&be_screen->frame_listener.link);
    wl_list_remove(&be_screen->output_destroyed_listener.link);
    wl_list_remove(&be_screen->link);
//cu_view:
    // TODO: free view
//cu_srfc:
    // TODO: free surface
//cu_screen:
    free(be_screen);
    return NULL;
}

static void handle_new_output(struct wl_listener *l, void *data){
    struct wlr_output *output = data;
    backend_t *be = wl_container_of(l, be, new_output_listener);

    // There's nothing we can do about errors here, so just ignore them.
    be_screen_new(be, output);
}

///// End Backend Screen Functions



int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
                  be_key_handler_t handler, void *data){
    return 0;
}

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h){
}

void be_unfocus_all(backend_t *be){
}

void be_window_focus(be_window_t *be_window){
}
void be_window_hide(be_window_t *be_window){
}
void be_window_show(be_window_t *be_window, be_screen_t *be_screen){
}
void be_window_close(be_window_t *be_window){
}
void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h){
}

// request an explicit repaint
void be_repaint(backend_t *be){
}

void backend_free(backend_t *be){
    wlr_xdg_shell_v6_destroy(be->xdg_shell_v6);
    wlr_compositor_destroy(be->compositor);
    wl_list_remove(&be->new_output_listener.link);
    wlr_backend_destroy(be->wlr_backend);
    wl_display_destroy(be->display);
    free(be);
}

backend_t *backend_new(void){
    backend_t *be = malloc(sizeof(*be));
    if(!be) return NULL;
    *be = (backend_t){0};

    be->display = wl_display_create();
    if(!be->display) goto fail_be;

    be->loop = wl_display_get_event_loop(be->display);
    if(!be->loop) goto fail_display;

    // get the wayland backend
    be->wlr_backend = wlr_backend_autocreate(be->display, NULL);
    if(!be->wlr_backend) goto fail_display;

    // get ready for some outputs
    wl_list_init(&be->be_screens);
    be->new_output_listener.notify = handle_new_output;
    wl_signal_add(&be->wlr_backend->events.new_output,
                  &be->new_output_listener);

    // add a compositor
    be->compositor = wlr_compositor_create(be->display,
            wlr_backend_get_renderer(be->wlr_backend));
    if(!be->compositor) goto fail_wlr_backend;

    // add some interfaces
    be->xdg_shell_v6 = wlr_xdg_shell_v6_create(be->display);
    if(!be->xdg_shell_v6) goto fail_compositor;

    wl_display_init_shm(be->display);

    // become a proper wayland server
    be->socket = wl_display_add_socket_auto(be->display);
    if(!be->socket) goto fail_xdg_shell_v6;

    return be;

fail_xdg_shell_v6:
    wlr_xdg_shell_v6_destroy(be->xdg_shell_v6);
fail_compositor:
    wlr_compositor_destroy(be->compositor);
fail_wlr_backend:
    wl_list_remove(&be->new_output_listener.link);
    wlr_backend_destroy(be->wlr_backend);
fail_display:
    wl_display_destroy(be->display);
fail_be:
    free(be);
    return NULL;
}

int backend_run(backend_t *be){
    // start the backend
    int ret = wlr_backend_start(be->wlr_backend);
    if(!ret) return -1;

    setenv("WAYLAND_DISPLAY", be->socket, true);

    exec("weston-info > wifo");

    wl_display_run(be->display);
    // TODO: check error
    return 0;
}

void backend_stop(backend_t *be){
}