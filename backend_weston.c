#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-util.h>

#include <compositor.h>
#include <compositor-drm.h>
#include <compositor-x11.h>
#include <windowed-output-api.h>

#include <libweston-desktop.h>

#include "backend.h"
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

/*
    // notes from the wayland-server-core.h

    typedef void (*wl_notify_func)(struct wl_listener *listener, void *data)

    struct wl_listener {
        struct wl_list link;
        wl_notify_func_t notify;
    }


    // woah, this is trippy:

    struct parent_struct {
        ... some stuff ...
        struct wl_listener listener_member;
    };
    void my_listener(struct wl_listener *listener, void *data){
        struct parent_struct ps;
        ps = wl_container_of(listener, ps, listener_member);
    }
*/

struct be_screen_t {
    struct weston_output *output;
    struct wl_listener output_destroyed;
    void *cb_data;
};

struct be_window_t {
    struct weston_desktop_surface *surface;
    struct weston_view *view;
    void *cb_data;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    bool dirty_geometry;
    // set when added to view_list, cleared after view unmap
    bool linked;
};

static backend_t *g_be;

struct backend_t {
    struct weston_compositor *compositor;
    struct wl_display *disp;
    struct wl_listener new_output_listener;
    struct wl_listener new_surface_listener;
    struct weston_drm_backend_config weston_drm_be_conf;
    struct weston_x11_backend_config weston_x11_be_conf;
    struct weston_layer layer_normal;
    struct weston_layer layer_bg;
    struct weston_surface *bg_srfc;
    struct weston_view *bg_view;
    struct weston_desktop *desktop;
    const struct weston_windowed_output_api *output_api;
    // for debug ////////////////////////////
	struct wl_listener destroy_signal;
	struct wl_listener create_surface_signal;
	struct wl_listener activate_signal;
	struct wl_listener transform_signal;
	struct wl_listener kill_signal;
	struct wl_listener idle_signal;
	struct wl_listener wake_signal;
	struct wl_listener show_input_panel_signal;
	struct wl_listener hide_input_panel_signal;
	struct wl_listener update_input_panel_signal;
	struct wl_listener seat_created_signal;
	struct wl_listener output_created_signal;
	struct wl_listener output_destroyed_signal;
	struct wl_listener output_moved_signal;
	struct wl_listener output_resized_signal;
	struct wl_listener output_heads_changed_signal;
	struct wl_listener session_signal;
	struct wl_listener heads_changed_signal;
    // end for-debug ////////////////////////////
};
// for debug ////////////////////////////
static void beh_destroy_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got destroy_signal\n");
}
static void beh_create_surface_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got create_surface_signal\n");
}
static void beh_activate_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got activate_signal\n");
}
static void beh_transform_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got transform_signal\n");
}
static void beh_kill_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got kill_signal\n");
}
static void beh_idle_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got idle_signal\n");
}
static void beh_wake_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got wake_signal\n");
}
static void beh_show_input_panel_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got show_input_panel_signal\n");
}
static void beh_hide_input_panel_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got hide_input_panel_signal\n");
}
static void beh_update_input_panel_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got update_input_panel_signal\n");
}
static void beh_seat_created_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got seat_created_signal\n");
}
static void beh_output_created_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got output_created_signal\n");
}
static void beh_output_destroyed_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got output_destroyed_signal\n");
}
static void beh_output_moved_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got output_moved_signal\n");
}
static void beh_output_resized_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got output_resized_signal\n");
}
static void beh_output_heads_changed_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got output_heads_changed_signal\n");
}
static void beh_session_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got session_signal\n");
}
static bool heads_changed_once = false;
static void beh_heads_changed_signal(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got heads_changed_signal\n");
    if(heads_changed_once) return;
    heads_changed_once = true;
    // create an output with the head
    backend_t *be = wl_container_of(l, be, heads_changed_signal);
    struct weston_head *head;
    wl_list_for_each(head, &be->compositor->head_list, compositor_link){
        logmsg("head!\n");
        struct weston_output *output =
            weston_compositor_create_output_with_head( be->compositor, head);
        weston_output_set_scale(output, 1);
        weston_output_set_transform(output, 0);
        be->output_api->output_set_size(output, 400, 400);
        weston_output_enable(output);
    }

}
// end for-debug ////////////////////////////

static void backend_handle_output_destroyed(struct wl_listener *l, void *data){
    (void)data;
    logmsg("backend_handle_output_destroyed()\n");
    be_screen_t *be_screen = wl_container_of(l, be_screen, output_destroyed);

    // call venowm's destroy screen handler
    handle_screen_destroy(be_screen->cb_data);

    free(be_screen);
}

static void backend_handle_output_new(struct wl_listener *l, void *data){
    logmsg("backend_handle_output_new()\n");
    struct weston_output *output = data;
    backend_t *be = wl_container_of(l, be, new_output_listener);

    // allocate a new be_screen_t
    be_screen_t *be_screen = malloc(sizeof(*be_screen));
    if(!be_screen) return;
    *be_screen = (be_screen_t){.output = output};

    // call venowm's new screen handler and get cb_data
    if(handle_screen_new(be_screen, &be_screen->cb_data)){
        // error!
        free(be_screen);
        return;
    }

    // set destroy handler
    be_screen->output_destroyed.notify = backend_handle_output_destroyed;
    wl_signal_add(&output->user_destroy_signal, &be_screen->output_destroyed);

    // create a window
    exec("env GDK_BACKEND=wayland vimb");

    // TODO: figure out how to handle resize events
    return;
}

static void backend_handle_compositor_exit(struct weston_compositor *c){
    logmsg("backend_handle_compositor_exit()\n");
    backend_t *be = weston_compositor_get_user_data(c);
    wl_display_terminate(be->disp);
}

static void backend_handle_surface_new(
        struct weston_desktop_surface *surface, void *user_data){
    logmsg("backend_handle_surface_new()\n");
    (void)user_data;

    // allocate window
    be_window_t *be_window = malloc(sizeof(*be_window));
    if(!be_window) goto fail;
    *be_window = (be_window_t){0};

    // store surface pointer
    be_window->surface = surface;
    // create a view
    be_window->view = weston_desktop_surface_create_view(surface);
    if(!be_window->view) goto fail_malloc;

    // call venowm's new window handler and store the cb_data
    if(handle_window_new(be_window, &be_window->cb_data)) goto fail_view;

    // store this be_window as the user_data for the surface
    weston_desktop_surface_set_user_data(surface, be_window);

    return;

fail_view:
    weston_view_destroy(be_window->view);
fail_malloc:
    free(be_window);
    // close the thing that just opened, or we will have a dangling window
fail:
    weston_desktop_surface_close(surface);
}

static void backend_handle_surface_destroy(
        struct weston_desktop_surface *surface, void *user_data){
    logmsg("backend_handle_surface_destroy()\n");
    // the callback's user_data is the backend_t
    (void)user_data;
    // the surface's user_data is the be_window_t
    be_window_t *be_window = weston_desktop_surface_get_user_data(surface);
    if(!be_window){
        // some error happened previously, just let this surface die in peace.
        return;
    }

    // call application's destroy handler
    handle_window_destroy(be_window->cb_data);

    // cleanup
    weston_view_destroy(be_window->view);
    free(be_window);
}

static struct weston_desktop_api desktop_api = {
    // for ABI backward-compatibility
    .struct_size=sizeof(struct weston_desktop_api),
    // minimal API requirements:
    .surface_added=backend_handle_surface_new,
    .surface_removed=backend_handle_surface_destroy,
};


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

    // set the log
    weston_log_set_handler(vlogmsg, vlogmsg);

    // create the compositor
    be->compositor = weston_compositor_create(be->disp, be);
    if(!be->compositor){
        logmsg("failed to allocate weston compositor\n");
        goto cu_display;
    }

    // configure the keyboard (?)
    if(weston_compositor_set_xkb_rule_names(be->compositor, NULL)){
        logmsg("failed to configure keyboard\n");
        goto cu_compositor;
    }
    /////////////////////////////

    // // load native backend
    // be->weston_be_conf = (struct weston_backend_config){
    //     .struct_version=WESTON_DRM_BACKEND_CONFIG_VERSION,
    //     .struct_size=sizeof(struct weston_backend_config)
    // };
    // if(weston_compositor_load_backend(be->compositor, WESTON_BACKEND_DRM,
    //                                   &be->weston_be_conf)){
    //     logmsg("failed to load weston backend\n");
    //     goto cu_compositor;
    // }
    //
    // // get the api for the backend
    // be->output_api = weston_drm_output_get_api(be->compositor);

    /////////////////////////////

    // load X11 backend
    be->weston_x11_be_conf = (struct weston_x11_backend_config){
        .base={.struct_version=WESTON_X11_BACKEND_CONFIG_VERSION,
               .struct_size=sizeof(struct weston_backend_config)}
    };
    if(weston_compositor_load_backend(be->compositor, WESTON_BACKEND_X11,
                                      &be->weston_x11_be_conf.base)){
        logmsg("failed to load weston backend\n");
        goto cu_compositor;
    }

    // get the api for the backend
    be->output_api = weston_windowed_output_get_api(be->compositor);

    // create an output
    be->output_api->create_head(be->compositor, "W1");

    /////////////////////////////

    // listen for created outputs
    be->new_output_listener.notify = backend_handle_output_new;
    wl_signal_add(&be->compositor->output_created_signal,
                  &be->new_output_listener);

    // catch the exit function
    be->compositor->exit = backend_handle_compositor_exit;

    // enable vt_switching
    be->compositor->vt_switching = true;

    // set up desktop (what even is this??)
    be->desktop = weston_desktop_create(be->compositor, &desktop_api, be);
    if(!be->desktop){
        goto cu_compositor;
    }

    // set up a layer (returns void)
    weston_layer_init(&be->layer_normal, be->compositor);
    weston_layer_set_position(&be->layer_normal, WESTON_LAYER_POSITION_NORMAL);
    g_be = be;

    // background layer
    weston_layer_init(&be->layer_bg, be->compositor);
    weston_layer_set_position(&be->layer_bg, WESTON_LAYER_POSITION_BACKGROUND);

// for debug ////////////////////////////
	be->destroy_signal.notify = beh_destroy_signal;
    wl_signal_add(&be->compositor->destroy_signal, &be->destroy_signal);
	be->create_surface_signal.notify = beh_create_surface_signal;
    wl_signal_add(&be->compositor->create_surface_signal, &be->create_surface_signal);
	be->activate_signal.notify = beh_activate_signal;
    wl_signal_add(&be->compositor->activate_signal, &be->activate_signal);
	be->transform_signal.notify = beh_transform_signal;
    wl_signal_add(&be->compositor->transform_signal, &be->transform_signal);
	be->kill_signal.notify = beh_kill_signal;
    wl_signal_add(&be->compositor->kill_signal, &be->kill_signal);
	be->idle_signal.notify = beh_idle_signal;
    wl_signal_add(&be->compositor->idle_signal, &be->idle_signal);
	be->wake_signal.notify = beh_wake_signal;
    wl_signal_add(&be->compositor->wake_signal, &be->wake_signal);
	be->show_input_panel_signal.notify = beh_show_input_panel_signal;
    wl_signal_add(&be->compositor->show_input_panel_signal, &be->show_input_panel_signal);
	be->hide_input_panel_signal.notify = beh_hide_input_panel_signal;
    wl_signal_add(&be->compositor->hide_input_panel_signal, &be->hide_input_panel_signal);
	be->update_input_panel_signal.notify = beh_update_input_panel_signal;
    wl_signal_add(&be->compositor->update_input_panel_signal, &be->update_input_panel_signal);
	be->seat_created_signal.notify = beh_seat_created_signal;
    wl_signal_add(&be->compositor->seat_created_signal, &be->seat_created_signal);
	be->output_created_signal.notify = beh_output_created_signal;
    wl_signal_add(&be->compositor->output_created_signal, &be->output_created_signal);
	be->output_destroyed_signal.notify = beh_output_destroyed_signal;
    wl_signal_add(&be->compositor->output_destroyed_signal, &be->output_destroyed_signal);
	be->output_moved_signal.notify = beh_output_moved_signal;
    wl_signal_add(&be->compositor->output_moved_signal, &be->output_moved_signal);
	be->output_resized_signal.notify = beh_output_resized_signal;
    wl_signal_add(&be->compositor->output_resized_signal, &be->output_resized_signal);
	be->output_heads_changed_signal.notify = beh_output_heads_changed_signal;
    wl_signal_add(&be->compositor->output_heads_changed_signal, &be->output_heads_changed_signal);
	be->session_signal.notify = beh_session_signal;
    wl_signal_add(&be->compositor->session_signal, &be->session_signal);
	be->heads_changed_signal.notify = beh_heads_changed_signal;
    wl_signal_add(&be->compositor->heads_changed_signal, &be->heads_changed_signal);
// end for-debug ////////////////////////////


    // make a blank background
    be->bg_srfc = weston_surface_create(be->compositor);
    if(!be->bg_srfc){
        goto cu_desktop;
    }
    weston_surface_set_size(be->bg_srfc, 8096, 8096);
    weston_surface_set_color(be->bg_srfc, 0.0, 0.0, 0.5, 1);

    // a view of the background
    be->bg_view = weston_view_create(be->bg_srfc);
    if(!be->bg_srfc){
        goto cu_bg_srfc;
    }
    weston_layer_entry_insert(&be->layer_bg.view_list,
                              &be->bg_view->layer_link);

    // damage surface
    weston_surface_damage(be->bg_srfc);

    return be;

cu_bg_srfc:
    weston_surface_destroy(be->bg_srfc);
cu_desktop:
    weston_desktop_destroy(be->desktop);
cu_compositor:
    weston_compositor_destroy(be->compositor);
cu_display:
    wl_display_destroy(be->disp);
cu_backend:
    free(be);
    return NULL;
}

void backend_free(backend_t *be){
    weston_view_destroy(be->bg_view);
    weston_surface_destroy(be->bg_srfc);
    weston_desktop_destroy(be->desktop);
    weston_compositor_destroy(be->compositor);
    wl_display_destroy(be->disp);
    free(be);
}

int backend_run(backend_t *be){
    logmsg("running\n");
    // get the compositor ready
    weston_compositor_wake(be->compositor);

    wl_display_run(be->disp);

    return 0;
}

void backend_stop(backend_t *be){
    weston_compositor_exit(be->compositor);
}

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
                  be_key_handler_t handler, void *data){
    (void)be; (void)mods; (void)key; (void)handler; (void)data;
    struct weston_binding *binding;
    binding = weston_compositor_add_key_binding(be->compositor, key, mods,
                                                handler, data);
    return -(binding == NULL);
}

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h){
    (void)be_screen; (void)x; (void)y; (void)w; (void)h;
}

void be_window_focus(be_window_t *be_window){
    logmsg("be_window_focus()\n");
    (void)be_window;
    // TODO: what to do when be_window is NULL?
    weston_desktop_surface_set_activated(be_window->surface, true);
}

void be_window_hide(be_window_t *be_window){
    logmsg("be_window_hide()\n");
    struct weston_surface *surface;
    surface = weston_desktop_surface_get_surface(be_window->surface);
    weston_surface_unmap(surface);
}

void be_window_show(be_window_t *be_window, be_screen_t *be_screen){
    logmsg("be_window_show()\n");
    // don't map already-mapped functions
    if(be_window->linked) return;
    logmsg("be_window_show...()\n");
    weston_view_set_output(be_window->view, be_screen->output);
    //weston_view_move_to_plane(be_window->view, &g_be->compositor->primary_plane);
    wl_list_insert(&g_be->compositor->view_list, &be_window->view->link);
    //weston_layer_entry_insert(&g_be->layer_normal.view_list, &be_window->view->layer_link);
    be_window->linked = true;
    // fix geometry of things which had their geometry set while they were unmapped
    if(!be_window->dirty_geometry) return;
    logmsg("be_window_show......()\n");
    weston_view_set_position(be_window->view, be_window->x, be_window->y);
    weston_desktop_surface_set_size(be_window->surface, be_window->w, be_window->h);
    be_window->dirty_geometry = false;
    // mark damage
    struct weston_surface *surface =
        weston_desktop_surface_get_surface(be_window->surface);
    weston_surface_damage(surface);
}

void be_window_close(be_window_t *be_window){
    logmsg("be_window_close()\n");
    weston_desktop_surface_close(be_window->surface);
}

void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h){
    logmsg("be_window_geometry()\n");
    be_window->x = x;
    be_window->y = y;
    be_window->w = w;
    be_window->h = h;
    be_window->dirty_geometry = true;
    // don't actually set the geometry of unlinked windows
    if(!be_window->linked) return;
    logmsg("be_window_geometry...()\n");
    weston_view_set_position(be_window->view, x, y);
    weston_desktop_surface_set_size(be_window->surface, w, h);
    be_window->dirty_geometry = false;
    // mark damage
    struct weston_surface *surface =
        weston_desktop_surface_get_surface(be_window->surface);
    weston_surface_damage(surface);
}
