#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <wayland-util.h>

#include <compositor.h>
#include <compositor-drm.h>
#include <compositor-x11.h>
#include <windowed-output-api.h>

#include <libweston-desktop.h>

#include <weston.h>

#include "backend.h"
#include "workspace.h"
#include "bindings.h"
#include "venowm.h"
#include "logmsg.h"

// venowm global variables
workspace_t *g_workspace;

screen_t **g_screens;
size_t g_screens_size;
size_t g_nscreens;

workspace_t **g_workspaces;
size_t g_workspaces_size;
size_t g_nworkspaces;


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
    backend_t *be;
    struct weston_output *output;
    struct wl_listener output_destroyed_listener;
    struct wl_list link; // link of backend_t->be_screens
    void *cb_data;
};

struct be_window_t {
    backend_t *be;
    // the surface backing this window
    struct weston_desktop_surface *surface;
    // the view of the window (currently only support a single one)
    struct weston_view *view;
    void *cb_data;
    int32_t x;
    int32_t y;
    uint32_t w;
    uint32_t h;
    bool dirty_geometry;
    // set in be_window_show, cleared in be_window_hide
    bool linked;
    struct wl_listener commit_listener;
};

struct be_seat_t {
    backend_t *be;
    struct weston_seat *seat;
    struct wl_listener seat_destroyed_listener;
    struct wl_list link; // link of backend_t->be_seats
};
typedef struct be_seat_t be_seat_t;

struct backend_t {
    struct weston_compositor *compositor;
    // compositor-level listeners
    struct wl_listener destroy_listener;
    struct wl_listener output_created_listener;
    struct wl_listener seat_created_listener;
    // layers
    struct weston_layer layer_normal;
    struct weston_layer layer_bg;
    struct weston_layer layer_minimized;
    // input
    struct text_backend *text_backend;

    struct weston_desktop *desktop;
    struct wl_list be_screens;
    struct wl_list be_seats;

    struct weston_desktop_surface *last_focused_surface;

    // old elements
    // struct wl_display *disp;
	// struct wl_listener heads_changed_listener;
};

///// Backend Screen Functions

static void be_screen_free(be_screen_t *be_screen){
    // call venowm's screen_destroy handler
    handle_screen_destroy(be_screen->cb_data);
    wl_list_remove(&be_screen->output_destroyed_listener.link);
    wl_list_remove(&be_screen->link);
    free(be_screen);
}

static void handle_output_destroyed(struct wl_listener *l, void *data){
    (void)data;
    be_screen_t *be_screen = wl_container_of(l, be_screen,
                                             output_destroyed_listener);
    be_screen_free(be_screen);
}

static be_screen_t *be_screen_new(backend_t *be, struct weston_output *output){
    be_screen_t *be_screen = malloc(sizeof(*be_screen));
    if(!be_screen) return NULL;
    be_screen->be = be;
    be_screen->output = output;

    // call venowm's new screen handler and get cb_data
    if(handle_screen_new(be_screen, &be_screen->cb_data)){
        // error!
        free(be_screen);
        return NULL;
    }

    be_screen->output_destroyed_listener.notify = handle_output_destroyed;
    wl_signal_add(&output->destroy_signal,
                  &be_screen->output_destroyed_listener);
    wl_list_insert(be->be_screens.prev, &be_screen->link);

    // TODO: handle resize/move events?

    // create a window
    logmsg("execing!\n");
    exec("env GDK_BACKEND=wayland vimb");

    return be_screen;
}

static void handle_output_created(struct wl_listener *l, void *data){
    logmsg("handle_output_created()\n");
    struct weston_output *output = data;
    backend_t *be = wl_container_of(l, be, output_created_listener);

    // There's nothing we can do about errors here, so just ignore them.
    be_screen_new(be, output);
}

///// End Backend Screen Functions

///// Backend Seat functions

static void be_seat_free(be_seat_t *be_seat){
    wl_list_remove(&be_seat->seat_destroyed_listener.link);
    wl_list_remove(&be_seat->link);
    free(be_seat);
}

static void handle_seat_destroyed(struct wl_listener *l, void *data){
    logmsg("handle_seat_destroyed()\n");
    (void)data;
    be_seat_t *be_seat = wl_container_of(l, be_seat, seat_destroyed_listener);
    be_seat_free(be_seat);
}

static be_seat_t *be_seat_new(backend_t *be, struct weston_seat *seat){
    be_seat_t *be_seat = malloc(sizeof(*be_seat));
    if(!be_seat) return NULL;
    be_seat->be = be;
    be_seat->seat = seat;

    // add destroy listener
    be_seat->seat_destroyed_listener.notify = handle_seat_destroyed;
    wl_signal_add(&seat->destroy_signal,
                  &be_seat->seat_destroyed_listener);

    // append to list
    wl_list_insert(be->be_seats.prev, &be_seat->link);

    return be_seat;

}

void handle_seat_created(struct wl_listener *l, void *data){
    logmsg("handle_seat_created()\n");
    struct weston_seat *seat = data;
    backend_t *be = wl_container_of(l, be, seat_created_listener);

    be_seat_new(be, seat);
}

///// End Backend Seat functions

///// Desktop API functions

static void backend_handle_surface_commit(struct wl_listener *l, void *data){
    logmsg("handle_surface_commit()\n");
    // be_window_t *be_window = wl_container_of(l, be_window, commit_listener);
    // backend_t *be = be_window->be;
    // // dereference the surface that did the commit
    // struct weston_surface *srfc = data;

    // // if the view is linked, damage the surface
    // if(be_window->linked){
    //     weston_surface_damage(srfc);
    //     weston_compositor_schedule_repaint(be->compositor);
    //     logmsg("scheduled repaint 1\n");
    // }
}

static void handle_desktop_surface_added(
        struct weston_desktop_surface *surface, void *user_data){
    logmsg("handle_desktop_surface_added()\n");
    // the callback's user_data is the backend_t
    backend_t *be = user_data;

    // allocate window
    be_window_t *be_window = malloc(sizeof(*be_window));
    if(!be_window) goto fail;
    *be_window = (be_window_t){0};

    // store the backend pointer
    be_window->be = be;
    // store surface pointer
    be_window->surface = surface;
    // create a view
    be_window->view = weston_desktop_surface_create_view(surface);
    if(!be_window->view) goto fail_malloc;

    // don't show the window just yet
    weston_layer_entry_insert(&be->layer_minimized.view_list,
                              &be_window->view->layer_link);
    be_window->linked = false;

    // call venowm's new window handler and store the cb_data
    if(handle_window_new(be_window, &be_window->cb_data)) goto fail_view;

    ////// No errors after this point

    weston_desktop_surface_set_fullscreen(surface, true);

    // store this be_window as the user_data for the surface
    weston_desktop_surface_set_user_data(surface, be_window);

    // add a listener to the commit signal for the surface
    be_window->commit_listener.notify = backend_handle_surface_commit;
    struct weston_surface *srfc = weston_desktop_surface_get_surface(surface);
    wl_signal_add(&srfc->commit_signal, &be_window->commit_listener);

    return;

fail_view:
    weston_desktop_surface_unlink_view(be_window->view);
    weston_view_destroy(be_window->view);
fail_malloc:
    free(be_window);
    // close the thing that just opened, or we will have a dangling window
fail:
    weston_desktop_surface_close(surface);
}

static void handle_desktop_surface_removed(
        struct weston_desktop_surface *surface, void *user_data){
    logmsg("handle_desktop_surface_removed()\n");
    // the callback's user_data is the backend_t
    backend_t *be = user_data;

    if(surface == be->last_focused_surface){
        be->last_focused_surface = NULL;
    }

    // the surface's user_data is the be_window_t
    be_window_t *be_window = weston_desktop_surface_get_user_data(surface);
    if(!be_window){
        // some error happened previously, just let this surface die in peace.
        return;
    }

    // call application's destroy handler
    handle_window_destroy(be_window->cb_data);

    // cleanup
    weston_desktop_surface_unlink_view(be_window->view);
    weston_view_destroy(be_window->view);
    free(be_window);
}

static const struct weston_desktop_api desktop_api = {
    // for ABI backward-compatibility
    .struct_size = sizeof(struct weston_desktop_api),
    // minimal API requirements:
    .surface_added = handle_desktop_surface_added,
    .surface_removed = handle_desktop_surface_removed,
};

///// End Desktop API functions

///// Plugin-level Functions

static void shell_free(backend_t *be){
    // cleanup be_seats
    {
        be_seat_t *be_seat;
        be_seat_t *temp;
        wl_list_for_each_safe(be_seat, temp, &be->be_seats, link){
            be_seat_free(be_seat);
        }
    }
    // cleanup be_screens
    {
        be_screen_t *be_screen;
        be_screen_t *temp;
        wl_list_for_each_safe(be_screen, temp, &be->be_screens, link){
            be_screen_free(be_screen);
        }
    }
    // cleanup desktop
    weston_desktop_destroy(be->desktop);
    // cleanup text_backend
    // TODO: text_backend respawns itself and I have no idea if this is valid
    text_backend_destroy(be->text_backend);
    // remove listeners
    wl_list_remove(&be->destroy_listener.link);
    // free backend struct
    free(be);

    // global variables:

    // screens are freed by the pre-destroy-screen handler
    FREE_PTR(g_screens, g_screens_size, g_nscreens);

    // but we have to manually free workspaces
    for(size_t i = 0; i < g_nworkspaces; i++){
        workspace_free(g_workspaces[i]);
    }
    FREE_PTR(g_workspaces, g_workspaces_size, g_nworkspaces);
}

static void handle_compositor_destroy(struct wl_listener *l, void *data){
    logmsg("handle_compositor_destroy()\n");
    (void)data;
    backend_t *be = wl_container_of(l, be, destroy_listener);
    shell_free(be);
}

int wet_shell_init(struct weston_compositor *c, int *argc, char *argv[]){
    (void)argc;
    (void)argv;
    int retval;
    int err;

    // allocate the backend
    backend_t *be = malloc(sizeof(*be));
    if(!be){
        logmsg("failed allocating backend\n");
        return -1;
    }
    *be = (backend_t){0};

    // listen to compositor-level events
    be->destroy_listener.notify = handle_compositor_destroy;
    wl_signal_add(&c->destroy_signal, &be->destroy_listener);
    // TODO: handle compositor's idle
    // TODO: handle compositor's wake
    // TODO: handle compositor's transform_signal? It seems to be for xwayland.

    // allocate global variables
    INIT_PTR(g_workspaces, g_workspaces_size, g_nworkspaces, 8, err);
    if(err){
        retval = -1;
        goto cu_be;
    }

    // allocate some workspaces
    for(size_t i = 0; i <= 5; i ++){
        workspace_t *new = workspace_new(be);
        if(!new){
            retval = -1;
            goto cu_workspaces;
        }
        int err;
        APPEND_PTR(g_workspaces, g_workspaces_size, g_nworkspaces, new, err);
        if(err){
            workspace_free(new);
            retval = -1;
            goto cu_workspaces;
        }
    }

    // set first workspace
    g_workspace = g_workspaces[0];

    INIT_PTR(g_screens, g_screens_size, g_nscreens, 4, err);
    if(err){
        retval = -1;
        goto cu_workspaces;
    }

    be->compositor = c;

    // layers
    weston_layer_init(&be->layer_normal, c);
    weston_layer_set_position(&be->layer_normal, WESTON_LAYER_POSITION_NORMAL);
    weston_layer_init(&be->layer_bg, c);
    weston_layer_set_position(&be->layer_bg, WESTON_LAYER_POSITION_BACKGROUND);
    weston_layer_init(&be->layer_minimized, c);
    weston_layer_set_position(&be->layer_minimized,
                              WESTON_LAYER_POSITION_HIDDEN);

    // text backend (honestly not sure what this even is)
    be->text_backend = text_backend_init(c);
    if(!be->text_backend){
        retval = -1;
        goto cu_screens;
    }

    // this is where libweston's example desktop-shell checks the config

    be->desktop = weston_desktop_create(c, &desktop_api, be);
    if(!be->desktop){
        retval = -1;
        goto cu_text_backend;
    }

    // setup each output
    struct weston_output *output;
    wl_list_init(&be->be_screens);
    wl_list_for_each(output, &c->output_list, link){
        be_screen_t *be_screen = be_screen_new(be, output);
        if(!be_screen){
            retval = -1;
            goto cu_be_screens;
        }
    }
    // be ready to handle new outputs dynamically
    be->output_created_listener.notify = handle_output_created;
    wl_signal_add(&c->output_created_signal, &be->output_created_listener);
    // TODO: handle output_move signal?

    // TODO: what the hell is launch_desktop_shell_process (shell.c:5155) for??

    // handle all of the seats
    struct weston_seat *seat;
    wl_list_init(&be->be_seats);
    wl_list_for_each(seat, &c->seat_list, link){
        be_seat_t *be_seat = be_seat_new(be, seat);
        if(!be_seat){
            retval = -1;
            goto cu_be_seats;
        }
    }
    // handle any new seats dynamically
    be->seat_created_listener.notify = handle_seat_created;
    wl_signal_add(&c->seat_created_signal, &be->seat_created_listener);

    // TODO: handle output_resized?

    if(add_bindings(be)) goto cu_be_seats;

    return 0;

cu_be_seats:
    {
        be_seat_t *be_seat;
        be_seat_t *temp;
        wl_list_for_each_safe(be_seat, temp, &be->be_seats, link){
            be_seat_free(be_seat);
        }
    }
cu_be_screens:
    {
        be_screen_t *be_screen;
        be_screen_t *temp;
        wl_list_for_each_safe(be_screen, temp, &be->be_screens, link){
            be_screen_free(be_screen);
        }
    }
// cu_desktop:
    weston_desktop_destroy(be->desktop);
cu_text_backend:
    // TODO: text_backend respawns itself and I have no idea if this is valid
    text_backend_destroy(be->text_backend);
cu_screens:
    // screens are freed by the pre-destroy-screen handler
    FREE_PTR(g_screens, g_screens_size, g_nscreens);
cu_workspaces:
    // but we have to manually free workspaces
    for(size_t i = 0; i < g_nworkspaces; i++){
        workspace_free(g_workspaces[i]);
    }
    FREE_PTR(g_workspaces, g_workspaces_size, g_nworkspaces);
cu_be:
    wl_list_remove(&be->destroy_listener.link);
    free(be);
    return retval;
}

///// End Plugin-level Functions

///// Public Functions

void backend_stop(backend_t *be){
    weston_compositor_exit(be->compositor);
}

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
                  be_key_handler_t handler, void *data){
    struct weston_binding *binding;
    binding = weston_compositor_add_key_binding(be->compositor, key, mods,
                                                handler, data);
    return -(binding == NULL);
}

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h){
    *x = be_screen->output->x;
    *y = be_screen->output->y;
    *w = (uint32_t)be_screen->output->width;
    *h = (uint32_t)be_screen->output->height;
}

void be_unfocus_all(backend_t *be){
    // un-focus the previously-focused surface
    if(be->last_focused_surface){
        weston_desktop_surface_set_activated(be->last_focused_surface, false);
        // mark damage
        struct weston_surface *srfc =
            weston_desktop_surface_get_surface(be->last_focused_surface);
        weston_surface_damage(srfc);
        weston_compositor_schedule_repaint(be->compositor);
        logmsg("scheduled repaint 2\n");
    }
    // no focus for every seat
    be_seat_t *be_seat;
    wl_list_for_each(be_seat, &be->be_seats, link){
        struct weston_keyboard *k;
        k = weston_seat_get_keyboard(be_seat->seat);
        if(k) weston_keyboard_set_focus(k, NULL);
    }
    be->last_focused_surface = NULL;
}

void be_window_focus(be_window_t *be_window){
    backend_t *be = be_window->be;
    // un-focus the previously-focused surface
    if(be->last_focused_surface){
        weston_desktop_surface_set_activated(
                be->last_focused_surface, false);
        // mark damage
        struct weston_surface *srfc =
            weston_desktop_surface_get_surface(be->last_focused_surface);
        weston_surface_damage(srfc);
        // schedule repaint after refocus
    }
    // focus the this next surface
    weston_desktop_surface_set_activated(be_window->surface, true);
    struct weston_surface *srfc;
    srfc = weston_desktop_surface_get_surface(be_window->surface);
    // mark damage
    weston_surface_damage(srfc);
    weston_compositor_schedule_repaint(be->compositor);
        logmsg("scheduled repaint\n");
    // focus every seat on surface
    struct be_seat_t *be_seat;
    wl_list_for_each(be_seat, &be->be_seats, link){
        struct weston_keyboard *k;
        k = weston_seat_get_keyboard(be_seat->seat);
        if(k) weston_keyboard_set_focus(k, srfc);
    }
    be->last_focused_surface = be_window->surface;
}

void be_window_hide(be_window_t *be_window){
    // don't mess with already-unmapped functions
    if(!be_window->linked) return;
    be_window->linked = false;
    // tell the application it's not activated
    weston_desktop_surface_set_activated(be_window->surface, false);
    // remove keyboard focus, if this surface had the focus
    struct weston_surface *srfc;
    srfc = weston_desktop_surface_get_surface(be_window->surface);
    be_seat_t *be_seat;
    wl_list_for_each(be_seat, &be_window->be->be_seats, link){
        struct weston_keyboard *k;
        k = weston_seat_get_keyboard(be_seat->seat);
        if(k && k->focus == srfc){
            weston_keyboard_set_focus(k, NULL);
        }
    }
    // remove from existing layer (should be normal layer)
    weston_layer_entry_remove(&be_window->view->layer_link);
    // insert into minimized layer
    weston_layer_entry_insert(&be_window->be->layer_minimized.view_list,
                              &be_window->view->layer_link);
}

void be_window_show(be_window_t *be_window, be_screen_t *be_screen){
    backend_t *be = be_window->be;
    // don't map already-mapped windows
    if(be_window->linked) return;
    weston_view_set_output(be_window->view, be_screen->output);
    // remove from existing layer (should be minimized)
    weston_layer_entry_remove(&be_window->view->layer_link);
    // insert into new layer (the normal layer)
    weston_layer_entry_insert(&be->layer_normal.view_list,
                              &be_window->view->layer_link);
    be_window->linked = true;

    // only set window geometry if it changed while window was unmapped
    if(!be_window->dirty_geometry) return;

    weston_desktop_surface_set_size(be_window->surface, be_window->w,
                                    be_window->h);
    weston_view_set_position(be_window->view, be_window->x, be_window->y);
    // Is this necessary?  Found it above struct weston_view
    // weston_view_geometry_dirty(be_window->view);
    be_window->dirty_geometry = false;
    // mark damage
    struct weston_surface *surface =
        weston_desktop_surface_get_surface(be_window->surface);
    weston_surface_damage(surface);
    weston_compositor_schedule_repaint(be->compositor);
        logmsg("scheduled repaint 3\n");
}

void be_window_close(be_window_t *be_window){
    weston_desktop_surface_close(be_window->surface);
}

void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h){
    backend_t *be = be_window->be;
    be_window->x = x;
    be_window->y = y;
    be_window->w = w;
    be_window->h = h;
    be_window->dirty_geometry = true;

    // don't actually set the geometry of unlinked windows
    if(!be_window->linked) return;

    weston_desktop_surface_set_size(be_window->surface, w, h);
    weston_view_set_position(be_window->view, x, y);
    be_window->dirty_geometry = false;
    // mark damage
    struct weston_surface *surface =
        weston_desktop_surface_get_surface(be_window->surface);
    weston_surface_damage(surface);
    weston_compositor_schedule_repaint(be->compositor);
        logmsg("scheduled repaint 4\n");
}
