#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <wayland-server.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/interface.h>

#include <xkbcommon/xkbcommon.h>

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

struct backend_t {
    struct wl_display *display;
    struct wl_event_loop *loop;
    // the wayland backend
    struct wlr_backend *wlr_backend;
    struct wl_listener new_input_device;
    // the wlr compositor
    struct wlr_compositor *compositor;
    struct wl_listener wlr_surface_created;
    struct wl_listener compositor_destroyed_listener;
    // shell interfaces
    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener xdg_shell_new_listener;
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
    backend_t *be = be_screen->be;

    be_screen_free(be_screen);

    // if that was the last screen, close venowm
    if(be->be_screens.next == &be->be_screens){
        backend_stop(be);
    }
}

static void handle_frame(struct wl_listener *l, void *data){
    (void)data;
    be_screen_t *be_screen = wl_container_of(l, be_screen, frame_listener);
    backend_t *be = be_screen->be;

    // wait, why the fuck are these two lines not the same???
    //struct wlr_output *o = be_screen->output; // segfaults
    struct wlr_output *o = data; // works

    struct wlr_renderer *r = wlr_backend_get_renderer(o->backend);

    // prepare the output for rendering
    int age = -1;
    wlr_output_make_current(o, &age);
    wlr_renderer_begin(r, o->width, o->height);

    // // render a blue square
    float color[4] = {0.0, 0.0, 0.5, 1.0};
    wlr_renderer_clear(r, color);

    // iterate over all the surface the compositor
    // TODO: track which surfaces are showing?
    struct wl_resource *resource;
    wl_resource_for_each(resource, &be->compositor->surface_resources){
        struct wlr_surface *surface = wlr_surface_from_resource(resource);
        if(!wlr_surface_has_buffer(surface)){
            continue;
        }
        struct wlr_box render_box = {
            .x = 20,
            .y = 20,
            .width = surface->current.width,
            .height = surface->current.height,
        };
        float mat[16];
        wlr_matrix_project_box((float*)&mat, &render_box,
                surface->current.transform, 0, (float*)&o->transform_matrix);
        struct wlr_texture *texture = wlr_surface_get_texture(surface);
        wlr_render_texture_with_matrix(r, texture, (float*)&mat, 1.0f);

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        wlr_surface_send_frame_done(surface, &now);

    }

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

    // create a global.  Not honestly sure what this is good for.
    wlr_output_create_global(output);

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


///// Input Functions

typedef struct {
    backend_t *be;
    struct wlr_input_device *device;
    struct wl_listener key_listener;
    struct wl_listener mod_listener;
} keyboard_t;

static void handle_key(struct wl_listener *l, void *data){
    keyboard_t *kbd = wl_container_of(l, kbd, key_listener);
    struct wlr_event_keyboard_key *event = data;
    (void)event; (void)kbd;
    logmsg("key\n");
}

static void handle_modifier(struct wl_listener *l, void *data){
    keyboard_t *kbd = wl_container_of(l, kbd, mod_listener);
    struct wlr_event_keyboard_modifier *event = data;
    (void)event; (void)kbd;
    logmsg("modifier\n");
}

static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;

keyboard_t *keyboard_new(backend_t *be, struct wlr_input_device *device){
    logmsg("new keyboard\n");
    keyboard_t *kbd = malloc(sizeof(*kbd));
    // if(!kbd) // TODO: what to do if malloc fails?
    *kbd = (keyboard_t){0};

    device->data = kbd;
    kbd->device = device;

    // load a keyboard map (generated with `xkbcomp $DISPLAY xkb.dump`)
    if(!xkb_context){
        xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        FILE *f = fopen("xkb.dump", "r");
        if(!f){
            perror("fopen(xkb.dump)");
        }else{
            xkb_keymap = xkb_keymap_new_from_file(
                    xkb_context, f, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
            fclose(f);
        }
    }


    wlr_keyboard_set_keymap(device->keyboard, xkb_keymap);

    kbd->key_listener.notify = handle_key;
    wl_signal_add(&device->keyboard->events.key, &kbd->key_listener);

    kbd->mod_listener.notify = handle_modifier;
    wl_signal_add(&device->keyboard->events.modifiers, &kbd->mod_listener);

    return kbd;
}

typedef struct {
    backend_t *be;
    struct wlr_input_device *device;
    struct wl_listener button_listener;
    struct wl_listener motion_listener;
} pointer_t;

static void handle_button(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, button_listener);
    struct wlr_event_mouse_button *event = data;
    (void)event; (void)ptr;
    logmsg("button\n");
}

static void handle_motion(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, motion_listener);
    struct wlr_event_mouse_motion *event = data;
    (void)event; (void)ptr;
    logmsg("motion\n");
}

pointer_t *pointer_new(backend_t *be, struct wlr_input_device *device){
    logmsg("new pointer\n");
    pointer_t *ptr = malloc(sizeof(*ptr));
    // if(!ptr) // TODO: what to do if malloc fails?
    *ptr = (pointer_t){0};

    device->data = ptr;
    ptr->device = device;

    ptr->button_listener.notify = handle_button;
    wl_signal_add(&device->pointer->events.button, &ptr->button_listener);

    ptr->motion_listener.notify = handle_motion;
    wl_signal_add(&device->pointer->events.motion_absolute,
                  &ptr->motion_listener);

    return ptr;
}




static void handle_new_input_device(struct wl_listener *l, void *data){
    backend_t *be = wl_container_of(l, be, new_input_device);
    struct wlr_input_device *device = data;

    switch(device->type){
        case WLR_INPUT_DEVICE_KEYBOARD:
            logmsg("input: WLR_INPUT_DEVICE_KEYBOARD\n");
            keyboard_new(be, device);
            break;
        case WLR_INPUT_DEVICE_POINTER:
            logmsg("input: WLR_INPUT_DEVICE_POINTER\n");
            pointer_new(be, device);
            break;
        case WLR_INPUT_DEVICE_TOUCH:
            logmsg("input: WLR_INPUT_DEVICE_TOUCH\n");
            break;
        case WLR_INPUT_DEVICE_TABLET_TOOL:
            logmsg("input: WLR_INPUT_DEVICE_TABLET_TOOL\n");
            break;
        case WLR_INPUT_DEVICE_TABLET_PAD:
            logmsg("input: WLR_INPUT_DEVICE_TABLET_PAD\n");
            break;
        case WLR_INPUT_DEVICE_SWITCH:
            logmsg("input: WLR_INPUT_DEVICE_SWITCH\n");
            break;
    }
}

///// End Input Functions


///// Backend Window Functions

struct be_window_t {
    backend_t *be;
    // base wl_surface, the first thing to be created
    struct wlr_surface *wlr_surface;
    struct wl_listener wlr_surface_destroyed;

    // xdg_surface, extends a wl_surface
    struct wlr_xdg_surface *xdg_surface;
    struct wl_listener xdg_surface_destroyed;
};

static void be_window_free(be_window_t *be_window){
    // don't need to remove destroy handlers
    free(be_window);
}

static void handle_wlr_surface_destroyed(struct wl_listener *l, void *data){
    (void)data;
    be_window_t *be_window;
    be_window = wl_container_of(l, be_window, wlr_surface_destroyed);
    be_window_free(be_window);
    logmsg("freed window (handle_wlr_surface_destroyed)!\n");
}

static be_window_t *be_window_new(backend_t *be,
                                  struct wlr_surface *wlr_surface){
    be_window_t *be_window = malloc(sizeof(*be_window));
    if(!be_window) return NULL;
    *be_window = (be_window_t){0};

    be_window->be = be;
    be_window->wlr_surface = wlr_surface;

    // add destroy handler
    be_window->wlr_surface_destroyed.notify = handle_wlr_surface_destroyed;
    wl_signal_add(&wlr_surface->events.destroy,
                  &be_window->wlr_surface_destroyed);

    return be_window;
}

static void handle_new_surface(struct wl_listener *l, void *data){
    struct wlr_surface *surface = data;
    backend_t *be = wl_container_of(l, be, wlr_surface_created);
    be_window_t *be_window = be_window_new(be, surface);
    // TODO: how to close this surface on errors?
    if(!be_window) return;

    logmsg("new surface!\n");
}

static void handle_xdg_shell_new(struct wl_listener *l, void *data){
    struct wlr_xdg_surface *surface = data;
    backend_t *be = wl_container_of(l, be, xdg_shell_new_listener);
    logmsg("new xdg surface!\n");
    (void)surface;
}

///// End Backend Window Functions


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
    wl_list_remove(&be->wlr_surface_created.link);
    wl_list_remove(&be->compositor_destroyed_listener.link);
    wl_list_remove(&be->xdg_shell_new_listener.link);
    wlr_xdg_shell_destroy(be->xdg_shell);
    // this gets called during compositor_destroyed
    // wlr_compositor_destroy(be->compositor);
    wl_list_remove(&be->new_output_listener.link);
    wlr_backend_destroy(be->wlr_backend);
    wl_display_destroy(be->display);
    free(be);
}

static void handle_compositor_destroyed(struct wl_listener *l, void *data){
    backend_t *be = wl_container_of(l, be, compositor_destroyed_listener);
    backend_free(be);
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
    be->new_input_device.notify = handle_new_input_device;
    wl_signal_add(&be->wlr_backend->events.new_input, &be->new_input_device);

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
    be->xdg_shell = wlr_xdg_shell_create(be->display);
    if(!be->xdg_shell) goto fail_compositor;
    be->xdg_shell_new_listener.notify = handle_xdg_shell_new;
    wl_signal_add(&be->xdg_shell->events.new_surface,
                  &be->xdg_shell_new_listener);

    wl_display_init_shm(be->display);

    // become a proper wayland server
    be->socket = wl_display_add_socket_auto(be->display);
    if(!be->socket) goto fail_xdg_shell;

    // add some listeners
    be->wlr_surface_created.notify = handle_new_surface;
    wl_signal_add(&be->compositor->events.new_surface,
                  &be->wlr_surface_created);
    be->compositor_destroyed_listener.notify = handle_compositor_destroyed;
    wl_signal_add(&be->compositor->events.destroy,
                  &be->compositor_destroyed_listener);

    return be;

// fail_listeners:
//     wl_list_remove(&be->wlr_surface_created.link);
//     wl_list_remove(&be->compositor_destroyed_listener.link);
fail_xdg_shell:
    wl_list_remove(&be->xdg_shell_new_listener.link);
    wlr_xdg_shell_destroy(be->xdg_shell);
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

    // exec("weston-info > wifo");
    exec("weston-terminal");

    wl_display_run(be->display);
    // TODO: check error
    return 0;
}

void backend_stop(backend_t *be){
    wl_display_terminate(be->display);
}
