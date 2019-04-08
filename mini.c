#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <xkbcommon/xkbcommon.h>
#include <linux/input.h>

#include <wayland-util.h>

#include <compositor.h>
#include <compositor-drm.h>
#include <compositor-x11.h>
#include <windowed-output-api.h>

#include <libweston-desktop.h>

#include "logmsg.h"

#define assert(cmd) if(!(cmd)) exit(10);

void exec(const char *shcmd){
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

struct weston_compositor *c;
struct wl_listener output_destroyed_listener;
struct wl_listener commit_listener;
struct weston_layer layer;
struct weston_view *view;
struct weston_output *output;
struct weston_seat *seat;
struct wl_listener keyboard_focus_listener;

void sigint_handler(int signum){
    logmsg("handled sigint\n");
    (void)signum;
    weston_compositor_exit(c);
}
static void simple_key_handler(struct weston_keyboard* keyboard,
                               const struct timespec* time,
                               uint32_t key,
                               void* data){
    (void)keyboard; (void)time; (void)key; (void)data;
    logmsg("got key!\n");
}

//static void quit_key(struct weston_keyboard_grab *grab,
//                     const struct timespec *time,
//                     uint32_t key, uint32_t state_w){
//    (void)grab; (void)time;
//    enum wl_keyboard_key_state state = state_w;
//    if(key == XKB_KEY_q && state == WL_KEYBOARD_KEY_STATE_PRESSED){
//        logmsg("got quit key!\n");
//    }
//}
//static void quit_modifier(struct weston_keyboard_grab *grab, uint32_t serial,
//                          uint32_t mods_depressed, uint32_t mods_latched,
//                          uint32_t mods_locked, uint32_t group){
//    logmsg("quit modifier!\n");
//}
//static void quit_cancel(struct weston_keyboard_grab *grab){
//    logmsg("quit cancel!\n");
//}
//static const struct weston_keyboard_grab_interface quit_grab_iface = {
//    quit_key,
//    quit_modifier,
//    quit_cancel,
//};


static bool heads_changed_once = false;
static void handle_heads_changed(struct wl_listener *l, void *data){
    (void)l; (void)data;
    logmsg("got heads_changed_signal\n");
    if(heads_changed_once) return;
    heads_changed_once = true;
    // create an output with the head
    struct weston_head *head;
    wl_list_for_each(head, &c->head_list, compositor_link){
        logmsg("head!\n");
        assert(!output);
        output = weston_compositor_create_output_with_head(c, head);
        assert(output);
        weston_output_set_scale(output, 1);
        weston_output_set_transform(output, 0);

        weston_windowed_output_get_api(c)->output_set_size(output, 400, 400);

        weston_output_enable(output);
    }

}

static void handle_output_destroyed(struct wl_listener *l, void *data){
    (void)l; (void)data;
}

static void handle_output_created(struct wl_listener *l, void *data){
    (void)l;
    logmsg("handle_output_created()\n");
    struct weston_output *output = data;
    // backend_t *be = wl_container_of(l, be, new_output_listener);

    // set destroy handler
    output_destroyed_listener.notify = handle_output_destroyed;
    wl_signal_add(&output->user_destroy_signal, &output_destroyed_listener);

    // set the transform of the output
    weston_output_set_transform(output, WL_OUTPUT_TRANSFORM_NORMAL);

    // create a window
    exec("env GDK_BACKEND=wayland vimb");

    // TODO: figure out how to handle resize events
    return;
}

static void handle_commit(struct wl_listener *l, void *data){
    (void)l;
    logmsg("handle_commit()\n");
    struct weston_surface *srfc = data;
    weston_surface_damage(srfc);
    weston_compositor_schedule_repaint(c);

    // struct weston_desktop_surface *surface =
    //     weston_surface_get_desktop_surface(srfc);
    // weston_desktop_surface_set_activated(surface, true);
}

static void handle_desktop_surface_added(
        struct weston_desktop_surface *surface, void *user_data){
    logmsg("handle_desktop_surface_added()\n");
    (void)user_data;
    // create a view
    assert(!view);
    view = weston_desktop_surface_create_view(surface);
    assert(view);
    // show the view
    weston_view_set_output(view, output);
    // necessary? desktop-shell/shell.c:focus_surface() does it:
    //view->is_mapped = true;

    // set geometry
    weston_desktop_surface_set_size(surface, 1366/2, 768);
    weston_view_set_position(view, output->x, output->y);

    weston_layer_entry_insert(&layer.view_list, &view->layer_link);

    // get plain weston_surface
    struct weston_surface *srfc = weston_desktop_surface_get_surface(surface);

    // activate the desktop surface
    weston_keyboard_set_focus(weston_seat_get_keyboard(seat), srfc);
    weston_desktop_surface_set_activated(surface, true);
    weston_desktop_surface_set_fullscreen(surface, true);

    // set damage
    // weston_surface_damage(srfc);
    // weston_compositor_schedule_repaint(c);

    // set commit handler
    commit_listener.notify = handle_commit;
    wl_signal_add(&srfc->commit_signal, &commit_listener);
}

static void handle_desktop_surface_removed(
        struct weston_desktop_surface *surface, void *user_data){
    (void)surface; (void)user_data;
    logmsg("handle_desktop_surface_removed()\n");
}

static void handle_compositor_exit(struct weston_compositor *c){
    logmsg("handle_compositor_exit()\n");
    struct wl_display *disp = weston_compositor_get_user_data(c);
    wl_display_terminate(disp);
}

static void handle_keyboard_focus(struct wl_listener *l, void *data){
    struct weston_keyboard *keyboard = data;
    struct weston_surface *srfc = keyboard->focus;
    (void)l; (void)keyboard;
    if(srfc){
        if(weston_surface_is_desktop_surface(srfc)){
            struct weston_desktop_surface *surface;
            surface = weston_surface_get_desktop_surface(srfc);
            weston_desktop_surface_set_activated(surface, true);
        }
    }
}

static struct weston_desktop_api desktop_api = {
    // for ABI backward-compatibility
    .struct_size=sizeof(struct weston_desktop_api),
    // minimal API requirements:
    .surface_added=handle_desktop_surface_added,
    .surface_removed=handle_desktop_surface_removed,
};

int main(){
    struct wl_display *disp = wl_display_create();
    assert(disp);

    const char *wl_sock = wl_display_add_socket_auto(disp);
    assert(wl_sock);

    assert(!setenv("WAYLAND_DISPLAY", wl_sock, 0));

    // set the log
    weston_log_set_handler(vlogmsg, vlogmsg);

    // create the compositor
    c = weston_compositor_create(disp, disp);
    assert(c);

    // configure the keyboard (?)
    assert(!weston_compositor_set_xkb_rule_names(c, NULL));

    // load X11 backend
    struct weston_x11_backend_config be_conf = {
        .base={.struct_version=WESTON_X11_BACKEND_CONFIG_VERSION,
               .struct_size=sizeof(struct weston_backend_config)}
    };
    assert(!weston_compositor_load_backend(c, WESTON_BACKEND_X11,
                                           &be_conf.base));

    // listen for heads changed
    struct wl_listener heads_changed_listener;
	heads_changed_listener.notify = handle_heads_changed;
    wl_signal_add(&c->heads_changed_signal,
                  &heads_changed_listener);

    // listen for created outputs
    struct wl_listener new_output_listener;
    new_output_listener.notify = handle_output_created;
    wl_signal_add(&c->output_created_signal, &new_output_listener);

    // create an output
    weston_windowed_output_get_api(c)->create_head(c, "W1");

    // catch the exit function
    c->exit = handle_compositor_exit;

    ////// use libweston-deskotp

    struct weston_desktop *desktop;
    desktop = weston_desktop_create(c, &desktop_api, NULL);
    assert(desktop);

    ////// get ready to run

    // set up a layer (returns void)
    weston_layer_init(&layer, c);
    weston_layer_set_position(&layer, WESTON_LAYER_POSITION_NORMAL);

    // make a blank background
    struct weston_surface *surface = weston_surface_create(c);
    assert(surface);
    weston_surface_set_size(surface, 300, 300);
    weston_surface_set_color(surface, 0.0, 0.0, 0.5, 1);

    // a view of the background
    struct weston_view *view = weston_view_create(surface);
    assert(view);
    weston_layer_entry_insert(&layer.view_list,
                              &view->layer_link);

    // damage surface
    weston_surface_damage(surface);

    // get the first seat from the compositor
    seat = NULL;
    wl_list_for_each(seat, &c->seat_list, link){
        break;
    }
    assert(seat);

    // listen for keyboard focus events
    keyboard_focus_listener.notify = handle_keyboard_focus;
    struct weston_keyboard *keyboard;
    keyboard = weston_seat_get_keyboard(seat);
    wl_signal_add(&keyboard->focus_signal, &keyboard_focus_listener);

    // get the compositor ready
    weston_compositor_wake(c);

    // struct weston_keyboard_grab quit_grab;
    // quit_grab.interface = &quit_grab_iface;
    // weston_keyboard_start_grab(keyboard, &quit_grab);

    weston_compositor_add_key_binding(c, KEY_Q, MODIFIER_CTRL,
                                      simple_key_handler, NULL);


    logmsg("---------\n");

    // set the SIGINT handler
    signal(SIGINT, sigint_handler);

    wl_display_run(disp);


    return 0;
}
