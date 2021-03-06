#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <wayland-server.h>
#include <wayland-util.h>

#include <wlr/backend.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/interface.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_server_decoration.h>

#include <xkbcommon/xkbcommon.h>

#include "backend.h"
#include "venowm.h"
#include "logmsg.h"
#include "venowm_control.h"

#include "venowm-shell-protocol.c"

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

    struct wl_list windows; // be_window.link
};

struct be_window_t {
    backend_t *be;
    void *venowm_data;
    // base wl_surface, the first thing to be created
    struct wlr_surface *wlr_surface;
    struct wl_listener wlr_surface_destroyed;

    // xdg_surface, extends a wl_surface
    struct wlr_xdg_surface *xdg_surface;
    bool mapped; // the application says if it's mapped or not
    struct wl_listener xdg_destroyed;
    struct wl_listener xdg_mapped;
    struct wl_listener xdg_unmapped;

    // display properties
    int32_t x;
    int32_t y;
    bool show; // we decide if the application is shown or not
    struct wl_list link; // be_screen_t.windows
};

/*
   KEY BINDING STRATEGY

   X keysyms are all within the 32-bit space with the some unused bits.  We can
   use this fact to make keybindings fit into a int32-keyed hash map of
   (X keysym + modifier)s to function/data pairs.
*/

enum be_modifier_t {
    // BE_MOD_      = 0x80000000 // skip the sign bit
    BE_MOD_CTRL  = 0x40000000,
    BE_MOD_SHIFT = 0x20000000,
    BE_MOD_ALT   = 0x0F000000,
    BE_MOD_SUPER = 0x04000000,
    // BE_MOD_      = 0x02000000
    // BE_MOD_      = 0x01000000
    // BE_MOD_      = 0x00400000
    // BE_MOD_      = 0x00200000
    // BE_MOD_      = 0x00100000
};

/*
   Frankly, this seems like not a great idea but I think it's cool and if it
   ever goes wrong it's easy enough to correct after the fact.
*/


typedef struct {
    // func should return "true" to consume the keypress
    bool (*func)(backend_t*, void*);
    void *data;
} keybinding_t;

// custom hashtable mapping (X keysym + modifier)s to function/data pairs
KHASH_INIT(keymap, int32_t, keybinding_t, true, kh_int_hash_func,
        kh_int_hash_equal);

typedef struct {
    khash_t(keymap) *bindings;
    backend_t *be;
    struct wl_list link; // backend_t.keymaps
} keymap_t;

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
    // xdg shell
    struct wlr_xdg_shell *xdg_shell;
    struct wl_listener xdg_shell_new_listener;
    // xdg decorations
    struct wlr_xdg_decoration_manager_v1 *decoration_mgr;
    struct wl_listener decoration_new;
    struct wl_listener decoration_mgr_destroy;
    // kde decorations
    struct wlr_server_decoration_manager *server_dec_mgr;
    // venowm control stuff
    venowm_control_t *vc;
    // outputs
    struct wlr_output_layout *output_layout;
    struct wl_listener new_output_listener;
    struct wl_list be_screens;
    const char *socket;
    // inputs
    struct wlr_seat *seat;
    uint32_t seat_caps;
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wl_list pointers; // pointer_t.link
    struct wl_list keyboards; // keyboard_t.link

    // keymaps
    keymap_t *keymap_top;
    struct wl_list keymaps; // keymap_t.link

    // for interacting with frontend
    be_window_t *focus;
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
    be_screen_t *be_screen = wl_container_of(
        l, be_screen, output_destroyed_listener);
    backend_t *be = be_screen->be;

    wlr_output_layout_remove(be->output_layout, be_screen->output);

    be_screen_free(be_screen);

    // if that was the last screen, close venowm
    if(be->be_screens.next == &be->be_screens){
        backend_stop(be);
    }
}

static void handle_frame(struct wl_listener *l, void *data){
    (void)data;
    be_screen_t *be_screen = wl_container_of(l, be_screen, frame_listener);
    struct wlr_output *o = be_screen->output;

    struct wlr_renderer *r = wlr_backend_get_renderer(o->backend);

    // prepare the output for rendering
    if(!wlr_output_attach_render(o, NULL)){
        return;
    }
    wlr_renderer_begin(r, o->width, o->height);

    // render a blue background
    float color[4] = {0.0, 0.0, 0.5, 1.0};
    wlr_renderer_clear(r, color);


    // render all the windows on this screen
    be_window_t *be_window;
    wl_list_for_each(be_window, &be_screen->windows, link){
        // don't render windows not mapped or not being shown
        if(!be_window->show || !be_window->mapped)
            continue;

        struct wlr_surface *srfc = be_window->wlr_surface;

        // don't render surfaces with no buffer
        if(!wlr_surface_has_buffer(srfc))
            continue;

        struct wlr_box render_box = {
            .x = be_window->x,
            .y = be_window->y,
            .width = srfc->current.width,
            .height = srfc->current.height,
        };
        float mat[16];
        wlr_matrix_project_box((float*)&mat, &render_box,
                srfc->current.transform, 0, (float*)&o->transform_matrix);
        struct wlr_texture *texture = wlr_surface_get_texture(srfc);
        wlr_render_texture_with_matrix(r, texture, (float*)&mat, 1.0f);

        struct timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        wlr_surface_send_frame_done(srfc, &now);
    }

    // show software cursor if hardware cursor is not working
    wlr_output_render_software_cursors(o, NULL);

    // TODO: set damage via wlr_output_set_damage()
    // (there don't seem to be any examples)

    // done rendering, commit buffer
    wlr_renderer_end(r);
    wlr_output_commit(o);
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

    // no windows on this screen yet
    wl_list_init(&be_screen->windows);

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

    be_screen_t *be_screen = be_screen_new(be, output);
    if(!be_screen) return;

    // how to handle more than one output?
    wlr_output_layout_add(be->output_layout, output, 0, 0);

    // if this isn't the right scale, load the right scale
    if(output->scale != 1.0){
        wlr_xcursor_manager_load(be->cursor_mgr, output->scale);
    }
}

///// End Backend Screen Functions


///// Input Functions

void keymap_free(keymap_t *keymap){
    wl_list_remove(&keymap->link);
    kh_destroy(keymap, keymap->bindings);
    free(keymap);
}

keymap_t *keymap_new(backend_t *be){
    // allocate keymap
    keymap_t *keymap = malloc(sizeof(*keymap));
    if(!keymap) return NULL;
    *keymap = (keymap_t){0};

    // allocate hash table
    keymap->bindings = kh_init(keymap);
    if(!keymap->bindings) goto fail;

    // add binding to backend's list
    keymap->be = be;
    wl_list_insert(be->keymaps.prev, &keymap->link);

    return keymap;

fail:
    free(keymap);
    return NULL;
}

// returns 0 on success, -1 on error
int keymap_add_keybinding(keymap_t *keymap, int32_t modded_keysym,
        bool (*func)(backend_t*, void*), void *data){
    // build a binding to be copied into the hashtable
    keybinding_t binding = {
        .func = func,
        .data = data,
    };

    // get index
    int ret;
    khiter_t k = kh_put(keymap, keymap->bindings, modded_keysym, &ret);
    if(ret < 0){
        return -1;
    }
    // write to index
    kh_value(keymap->bindings, k) = binding;

    return 0;
}

static const keybinding_t KEYBINDING_NONE = (keybinding_t){0};

// return KEYBINDING_NONE if there is no binding
keybinding_t keymap_get_keybinding(keymap_t *keymap, int32_t modded_keysym){
    // get index
    khiter_t k = kh_get(keymap, keymap->bindings, modded_keysym);
    // check if value is missing
    int is_missing = (k == kh_end(keymap->bindings));
    if(is_missing){
        return KEYBINDING_NONE;
    }
    // return value
    return kh_value(keymap->bindings, k);
}

typedef struct {
    backend_t *be;
    struct wlr_input_device *device;
    struct wl_listener key_listener;
    struct wl_listener mod_listener;
    struct wl_listener keyboard_destroyed;
    struct wl_list link; // backend_t.keyboards
} keyboard_t;

// return "true" to consume the keybinding
static bool keymap_filter_keysyms(keymap_t *keymap, uint32_t mods,
        const xkb_keysym_t *keysyms, size_t nkeysyms){
    for(size_t i = 0; i < nkeysyms; i++){
        int32_t modded_keysym = keysyms[i];
        // capture modifiers we care about
        if(mods & WLR_MODIFIER_SHIFT)   modded_keysym |= BE_MOD_SHIFT;
        if(mods & WLR_MODIFIER_CTRL)    modded_keysym |= BE_MOD_CTRL;
        if(mods & WLR_MODIFIER_ALT)     modded_keysym |= BE_MOD_ALT;
        if(mods & WLR_MODIFIER_MOD3)    modded_keysym |= BE_MOD_SUPER;

        keybinding_t binding = keymap_get_keybinding(keymap, modded_keysym);

        if(!binding.func) continue;

        // call the first keybinding
        return binding.func(keymap->be, binding.data);
    }
    return false;
}

// return "true" to consume the keybinding
static bool keymap_filter_keycode(backend_t *be, struct wlr_keyboard *k,
        xkb_keycode_t keycode){
    uint32_t modifiers;
    const xkb_keysym_t *keysyms;
    size_t nkeysyms;

    // try and read "translated" keysyms such as "s-bar" for "SUPER+SHIFT+\"

    // get modifiers
    modifiers = wlr_keyboard_get_modifiers(k);
    // see which modifiers are already consumed
    xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(k->xkb_state,
            keycode, XKB_CONSUMED_MODE_XKB);
    modifiers &= ~consumed;
    // get syms
    nkeysyms = xkb_state_key_get_syms(k->xkb_state, keycode, &keysyms);
    // check it against the keymap
    if(keymap_filter_keysyms(be->keymap_top, modifiers, keysyms, nkeysyms)){
        return true;
    }

    // try and read "raw" keysyms such as "s-shift+\" for "SUPER+SHIFT+\"

    modifiers = wlr_keyboard_get_modifiers(k);
    xkb_layout_index_t layout_index = xkb_state_key_get_layout(k->xkb_state,
            keycode);
    nkeysyms = xkb_keymap_key_get_syms_by_level(k->keymap, keycode,
            layout_index, 0, &keysyms);
    if(keymap_filter_keysyms(be->keymap_top, modifiers, keysyms, nkeysyms)){
        return true;
    }

    return false;
}


static void handle_key(struct wl_listener *l, void *data){
    keyboard_t *kbd = wl_container_of(l, kbd, key_listener);
    struct wlr_event_keyboard_key *event = data;
    struct wlr_keyboard *k = kbd->device->keyboard;
    backend_t *be = kbd->be;

    xkb_keycode_t keycode = event->keycode + 8;

    if(event->state == WLR_KEY_PRESSED){
        if(keymap_filter_keycode(be, k, keycode)){
            return;
        }
    }

    // wlr_seat_set_keyboard() is a noop if this keyboard is already set
    wlr_seat_set_keyboard(be->seat, kbd->device);

    // send key through seat
    wlr_seat_keyboard_notify_key(be->seat, event->time_msec, event->keycode,
            event->state);
}

static void handle_modifier(struct wl_listener *l, void *data){
    (void)data;
    keyboard_t *kbd = wl_container_of(l, kbd, mod_listener);
    backend_t *be = kbd->be;

    // wlr_seat_set_keyboard() is a noop if this keyboard is already set
    wlr_seat_set_keyboard(be->seat, kbd->device);

    // send modifier through seat
    wlr_seat_keyboard_notify_modifiers(be->seat,
            &kbd->device->keyboard->modifiers);
}

static void keyboard_destroyed(struct wl_listener *l, void *data){
    keyboard_t *kbd = wl_container_of(l, kbd, keyboard_destroyed);
    (void)data;
    backend_t *be = kbd->be;

    // remove from backend list
    wl_list_remove(&kbd->link);

    if(wl_list_empty(&be->keyboards)
            && (be->seat_caps & WL_SEAT_CAPABILITY_KEYBOARD)){
        be->seat_caps &= ~WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_seat_set_capabilities(be->seat, be->seat_caps);
    }
}

static struct xkb_context *xkb_context = NULL;
static struct xkb_keymap *xkb_keymap = NULL;
static struct xkb_rule_names rules = {0};

static keyboard_t *keyboard_new(backend_t *be,
        struct wlr_input_device *device){
    logmsg("new keyboard\n");
    keyboard_t *kbd = malloc(sizeof(*kbd));
    if(!kbd) return NULL;
    *kbd = (keyboard_t){0};

    device->data = kbd;
    kbd->device = device;
    kbd->be = be;


    // // load a keyboard map (generated with `xkbcomp $DISPLAY xkb.dump`)
    // if(!xkb_context){
    //     xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    //     FILE *f = fopen("xkb.dump", "r");
    //     if(!f){
    //         perror("fopen(xkb.dump)");
    //     }else{
    //         xkb_keymap = xkb_keymap_new_from_file(
    //                 xkb_context, f, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
    //         fclose(f);
    //     }
    // }

    // use default xkb rules
    if(!xkb_context){
        xkb_context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        if(!xkb_context) goto fail;
    }
    if(!xkb_keymap){
        xkb_keymap = xkb_keymap_new_from_names(
            xkb_context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);
        if(!xkb_keymap) goto fail;
    }

    wlr_keyboard_set_keymap(device->keyboard, xkb_keymap);
    wlr_keyboard_set_repeat_info(device->keyboard, 40, 200);

    kbd->key_listener.notify = handle_key;
    wl_signal_add(&device->keyboard->events.key, &kbd->key_listener);

    kbd->mod_listener.notify = handle_modifier;
    wl_signal_add(&device->keyboard->events.modifiers, &kbd->mod_listener);

    kbd->keyboard_destroyed.notify = keyboard_destroyed;
    wl_signal_add(&device->events.destroy, &kbd->keyboard_destroyed);

    // add to backend list
    wl_list_insert(be->keyboards.prev, &kbd->link);

    if(!(be->seat_caps & WL_SEAT_CAPABILITY_KEYBOARD)){
        be->seat_caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_seat_set_capabilities(be->seat, be->seat_caps);
    }

    wlr_seat_set_keyboard(kbd->be->seat, kbd->device);

    return kbd;

fail:
    free(kbd);
    return NULL;
}

typedef struct {
    backend_t *be;
    struct wlr_input_device *device;
    struct wl_listener button_listener;
    struct wl_listener motion_listener;
    struct wl_listener motion_abs_listener;
    struct wl_listener pointer_destroyed;
    struct wl_list link; // backend_t.pointers
} pointer_t;

static void handle_button(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, button_listener);
    struct wlr_event_mouse_button *event = data;
    (void)event; (void)ptr;
    // logmsg("button\n");
}

static void handle_motion(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, motion_listener);
    struct wlr_event_pointer_motion *event = data;
    backend_t *be = ptr->be;
    // logmsg("motion\n");

    wlr_cursor_move(be->cursor, event->device, event->delta_x, event->delta_y);
}

static void handle_motion_abs(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, motion_abs_listener);
    struct wlr_event_pointer_motion_absolute *event = data;
    backend_t *be = ptr->be;

    wlr_xcursor_manager_set_cursor_image(
        be->cursor_mgr, "left_ptr", be->cursor);

    double x;
    double y;
    wlr_cursor_absolute_to_layout_coords(
        be->cursor, event->device, event->x, event->y, &x, &y);

    // logmsg("motion_abs (%.3f, %.3f) -> (%.3f, %.3f)\n",
    //     event->x, event->y, x, y);

    wlr_cursor_warp_closest(be->cursor, event->device, x, y);
}

static void pointer_destroyed(struct wl_listener *l, void *data){
    pointer_t *ptr = wl_container_of(l, ptr, pointer_destroyed);
    (void)data;
    backend_t *be = ptr->be;

    // remove from backend list
    wl_list_remove(&ptr->link);

    if(wl_list_empty(&be->pointers)
            && (be->seat_caps & WL_SEAT_CAPABILITY_POINTER)){
        be->seat_caps &= ~WL_SEAT_CAPABILITY_POINTER;
        wlr_seat_set_capabilities(be->seat, be->seat_caps);
    }
}

static pointer_t *pointer_new(backend_t *be, struct wlr_input_device *device){
    logmsg("new pointer\n");
    pointer_t *ptr = malloc(sizeof(*ptr));
    if(!ptr) return NULL;
    *ptr = (pointer_t){0};

    device->data = ptr;
    ptr->device = device;
    ptr->be = be;

    // listen to events

    ptr->button_listener.notify = handle_button;
    wl_signal_add(&device->pointer->events.button, &ptr->button_listener);

    ptr->motion_listener.notify = handle_motion;
    wl_signal_add(&device->pointer->events.motion,
                  &ptr->motion_listener);

    ptr->motion_abs_listener.notify = handle_motion_abs;
    wl_signal_add(&device->pointer->events.motion_absolute,
                  &ptr->motion_abs_listener);

    ptr->pointer_destroyed.notify = pointer_destroyed;
    wl_signal_add(&device->events.destroy, &ptr->pointer_destroyed);

    // add pointer to cursor
    wlr_cursor_attach_input_device(be->cursor, device);

    // add to backend list
    wl_list_insert(be->pointers.prev, &ptr->link);

    if(!(be->seat_caps & WL_SEAT_CAPABILITY_POINTER)){
        be->seat_caps |= WL_SEAT_CAPABILITY_POINTER;
        wlr_seat_set_capabilities(be->seat, be->seat_caps);
    }

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
    struct wlr_surface *wlr_surface = data;
    backend_t *be = wl_container_of(l, be, wlr_surface_created);
    be_window_t *be_window = be_window_new(be, wlr_surface);

    logmsg("new wlr_surface! %p\n", wlr_surface);

    // we can't close the surface until it becomes an xdg_shell, so let NULL
    // indicate that the surface is a total loser who deserves to be shutdown

    wlr_surface->data = be_window;
}

static void handle_xdg_destroyed(struct wl_listener *l, void *data){
    be_window_t *be_window = wl_container_of(l, be_window, xdg_destroyed);
    backend_t *be = be_window->be;

    // remove focus if it had focus
    if(be->focus == be_window){
        // point keyboard at nothing
        wlr_seat_keyboard_clear_focus(be->seat);
        // no more focus
        be->focus = NULL;
    }
}

static void handle_xdg_mapped(struct wl_listener *l, void *data){
    be_window_t *be_window = wl_container_of(l, be_window, xdg_mapped);

    be_window->mapped = true;

    logmsg("xdg mapped\n");

    // set XDG to be fully tiled
    wlr_xdg_toplevel_set_tiled(be_window->xdg_surface, 15);

    // call hook into venowm
    handle_window_new(be_window, &be_window->venowm_data);
}

static void handle_xdg_unmapped(struct wl_listener *l, void *data){
    be_window_t *be_window = wl_container_of(l, be_window, xdg_unmapped);

    be_window->mapped = false;

    logmsg("xdg unmapped\n");

    /* TODO: can this happen any time, or only right before the application
       closes?  If it can happen frequently, it might not be good to unfocus
       the window.  This would be like some kind of guard against redraws. If
       it happens rarely or only before close than this should affect things
       like if the application is activated and where the keyboard focus is.

       By not adjusting focus or activation here, we are kinda assuming one,
       but by calling handle_window_destroy we are kinda assuming the other.
    */

    // call hook into venowm
    if(be_window->venowm_data){
        handle_window_destroy(be_window->venowm_data);
    }
}

// We have an xdg surface but it doesn't necessarily have a role yet.
static void handle_xdg_shell_new(struct wl_listener *l, void *data){
    struct wlr_xdg_surface *xdg_surface = data;
    backend_t *be = wl_container_of(l, be, xdg_shell_new_listener);

    logmsg("new xdg_surface! %p\n", xdg_surface);

    // get the be_window from the wlr_surface under the xdg_surface
    be_window_t *be_window = xdg_surface->surface->data;

    // if we got here but no be_window was allocated, close the application
    if(!be_window){
        //// Wait, this isn't valid because the surface has no role yet.
        // wlr_xdg_toplevel_send_close(xdg_surface);
        // return;
        // TODO: shit, we don't really have a way to handle this.
        exit(101);
    }

    // store the xdg_surface within the be_window
    be_window->xdg_surface = xdg_surface;

    // hooks for the xdg_surface
    be_window->xdg_destroyed.notify = handle_xdg_destroyed;
    wl_signal_add(&xdg_surface->events.destroy, &be_window->xdg_destroyed);

    be_window->xdg_mapped.notify = handle_xdg_mapped;
    wl_signal_add(&xdg_surface->events.map, &be_window->xdg_mapped);

    be_window->xdg_unmapped.notify = handle_xdg_unmapped;
    wl_signal_add(&xdg_surface->events.unmap, &be_window->xdg_unmapped);

    // initial state
    be_window->mapped = false;

    // don't call into venowm until the surface is mapped
}

///// End Backend Window Functions


///// Decoration Functions

void handle_decoration_new(struct wl_listener *l, void *data){
    backend_t *be = wl_container_of(l, be, decoration_new);
    struct wlr_xdg_toplevel_decoration_v1 *dec = data;

    /* TODO: do we need to actually handle requests from the decoration? Or can
             we just tell the decoration to use server-side decorations and
             call it good?  Let's give that a shot. */
    logmsg("new decoration!!!!!!!!!!!!!!!!!!!!!!!!\n");

    // Fire and forget.  Probably the return value means something.
    uint32_t ret = wlr_xdg_toplevel_decoration_v1_set_mode(dec,
            WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    (void)ret;
}

void handle_decoration_mgr_destroy(struct wl_listener *l, void *data){
    (void)data;
    backend_t *be = wl_container_of(l, be, decoration_mgr_destroy);
    /* TODO: should destroy() get called here or in backend free?  Not both.
             Actually, I would consider shutting down the whole compositor if
             this died a valid plan.  It shouldn't die. */
    (void)be;
}

///// End Decoration Functions


void backend_free(backend_t *be){
    // free all the keymaps
    {
        keymap_t *keymap;
        keymap_t *temp;
        wl_list_for_each_safe(keymap, temp, &be->keymaps, link){
            keymap_free(keymap);
        }
    }
    wlr_xcursor_manager_destroy(be->cursor_mgr);
    wlr_cursor_destroy(be->cursor);
    wlr_seat_destroy(be->seat);
    wl_list_remove(&be->wlr_surface_created.link);
    wl_list_remove(&be->compositor_destroyed_listener.link);
    venowm_control_free(be->vc);
    wlr_server_decoration_manager_destroy(be->server_dec_mgr);
    wl_list_remove(&be->decoration_new.link);
    wl_list_remove(&be->decoration_mgr_destroy.link);
    wlr_xdg_decoration_manager_v1_destroy(be->decoration_mgr);
    wl_list_remove(&be->xdg_shell_new_listener.link);
    wlr_xdg_shell_destroy(be->xdg_shell);
    // TODO: fix the order of things here
    // this gets called during compositor_destroyed
    // wlr_compositor_destroy(be->compositor);
    wlr_output_layout_destroy(be->output_layout);
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

    be->output_layout = wlr_output_layout_create();
    if(!be->output_layout) goto fail_wlr_backend;

    // add a compositor
    be->compositor = wlr_compositor_create(be->display,
            wlr_backend_get_renderer(be->wlr_backend));
    if(!be->compositor) goto fail_output_layout;

    // xdg_shell interface
    be->xdg_shell = wlr_xdg_shell_create(be->display);
    if(!be->xdg_shell) goto fail_compositor;
    be->xdg_shell_new_listener.notify = handle_xdg_shell_new;
    wl_signal_add(&be->xdg_shell->events.new_surface,
                  &be->xdg_shell_new_listener);

    // shared memory stuff
    wl_display_init_shm(be->display);

    // xdg_decoration stuff
    be->decoration_mgr = wlr_xdg_decoration_manager_v1_create(be->display);
    if(!be->decoration_mgr) goto fail_xdg_shell;
    be->decoration_new.notify = handle_decoration_new;
    wl_signal_add(&be->decoration_mgr->events.new_toplevel_decoration,
                  &be->decoration_new);
    be->decoration_mgr_destroy.notify = handle_decoration_mgr_destroy;
    wl_signal_add(&be->decoration_mgr->events.destroy,
                  &be->decoration_mgr_destroy);

    // kde server decoration stuff
    be->server_dec_mgr = wlr_server_decoration_manager_create(be->display);
    if(!be->server_dec_mgr) goto fail_decoration_mgr;
    wlr_server_decoration_manager_set_default_mode(
            be->server_dec_mgr, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

    // venowm_control interface
    be->vc = venowm_control_new(be, be->display);
    if(!be->vc) goto fail_server_dec_mgr;

    // become a proper wayland server
    be->socket = wl_display_add_socket_auto(be->display);
    if(!be->socket) goto fail_venowm_control;

    // add some listeners
    be->wlr_surface_created.notify = handle_new_surface;
    wl_signal_add(&be->compositor->events.new_surface,
                  &be->wlr_surface_created);
    be->compositor_destroyed_listener.notify = handle_compositor_destroyed;
    wl_signal_add(&be->compositor->events.destroy,
                  &be->compositor_destroyed_listener);

    // add a seat
    be->seat = wlr_seat_create(be->display, "seat-name");
    if(!be->seat) goto fail_listeners;

    // prepare input lists
    wl_list_init(&be->pointers);
    wl_list_init(&be->keyboards);
    // no keyboards or pointers yet
    be->seat_caps = 0;

    // add a cursor
    be->cursor = wlr_cursor_create();
    if(!be->cursor) goto fail_seat;
    wlr_cursor_attach_output_layout(be->cursor, be->output_layout);

    // add a cursor manger
    be->cursor_mgr = wlr_xcursor_manager_create("default", 24);
    if(!be->cursor_mgr) goto fail_cursor;

    // load the default scale here
    wlr_xcursor_manager_load(be->cursor_mgr, 1.0);
    wlr_xcursor_manager_set_cursor_image(
        be->cursor_mgr, "left_ptr", be->cursor);

    // prepare keymaps
    wl_list_init(&be->keymaps);
    be->keymap_top = keymap_new(be);
    if(!be->keymap_top) goto fail_cursor_mgr;

    return be;

// fail_keymaps:
//     {
//         keymap_t *keymap;
//         keymap_t *temp;
//         wl_list_for_each_safe(keymap, temp, &be->keymaps, link){
//             keymap_free(keymap);
//         }
//     }
fail_cursor_mgr:
    wlr_xcursor_manager_destroy(be->cursor_mgr);
fail_cursor:
    wlr_cursor_destroy(be->cursor);
fail_seat:
    wlr_seat_destroy(be->seat);
fail_listeners:
    wl_list_remove(&be->wlr_surface_created.link);
    wl_list_remove(&be->compositor_destroyed_listener.link);
fail_venowm_control:
    venowm_control_free(be->vc);
fail_server_dec_mgr:
    wlr_server_decoration_manager_destroy(be->server_dec_mgr);
fail_decoration_mgr:
    wl_list_remove(&be->decoration_new.link);
    wl_list_remove(&be->decoration_mgr_destroy.link);
    wlr_xdg_decoration_manager_v1_destroy(be->decoration_mgr);
fail_xdg_shell:
    wl_list_remove(&be->xdg_shell_new_listener.link);
    wlr_xdg_shell_destroy(be->xdg_shell);
fail_compositor:
    wlr_compositor_destroy(be->compositor);
fail_output_layout:
    wlr_output_layout_destroy(be->output_layout);
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

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
        bool (*func)(backend_t*, void*), void *data){
    // get the modded_key for the hashtable
    uint32_t modded_keysym = (uint32_t)key;
    if(mods & MOD_CTRL)     modded_keysym |= BE_MOD_CTRL;
    if(mods & MOD_SHIFT)    modded_keysym |= BE_MOD_SHIFT;
    if(mods & MOD_ALT)      modded_keysym |= BE_MOD_ALT;
    if(mods & MOD_SUPER)    modded_keysym |= BE_MOD_SUPER;

    int ret;
    ret = keymap_add_keybinding(be->keymap_top, modded_keysym, func, data);
    if(ret < 0){
        return -1;
    }

    return 0;
}

void be_screen_get_geometry(be_screen_t *be_screen, int32_t *x, int32_t *y,
        uint32_t *w, uint32_t *h){
    *x = 0;
    *y = 0;
    *w = (uint32_t)be_screen->output->width;
    *h = (uint32_t)be_screen->output->height;
}

void be_unfocus_all(backend_t *be){
    if(be->focus != NULL){
        // deactivate surface
        wlr_xdg_toplevel_set_activated(be->focus->xdg_surface, false);
        // point keyboard at nothing
        wlr_seat_keyboard_clear_focus(be->seat);
        // no more focus
        be->focus = NULL;
    }
}

void be_window_focus(be_window_t *be_window){
    backend_t *be = be_window->be;
    if(be->focus != NULL){
        // deactivate old surface
        wlr_xdg_toplevel_set_activated(be->focus->xdg_surface, false);
    }

    // activate new focus
    wlr_xdg_toplevel_set_activated(be_window->xdg_surface, true);

    // point keyboard at new surface
    struct wlr_keyboard *kbd = wlr_seat_get_keyboard(be->seat);
    if(kbd){
        wlr_seat_keyboard_notify_enter(be->seat, be_window->wlr_surface,
                kbd->keycodes, kbd->num_keycodes, &kbd->modifiers);
    }

    be->focus = be_window;
    logmsg("new focus window's role: %d\n", be_window->xdg_surface->role);
}

void be_window_hide(be_window_t *be_window){
    backend_t *be = be_window->be;
    if(!be_window->show) return;
    be_window->show = false;
    wl_list_remove(&be_window->link);
    // if the window was focused, unfocus it
    if(be->focus == be_window){
        be_unfocus_all(be);
    }
}

void be_window_show(be_window_t *be_window, be_screen_t *be_screen){
    if(be_window->show) return;
    be_window->show = true;
    // add this window to that screen
    wl_list_insert(be_screen->windows.prev, &be_window->link);
}

void be_window_close(be_window_t *be_window){
    wlr_xdg_toplevel_send_close(be_window->xdg_surface);
    // TODO: handle popups as well
}

void be_window_geometry(be_window_t *be_window, int32_t x, int32_t y,
        uint32_t w, uint32_t h){
    uint32_t serial = wlr_xdg_toplevel_set_size(be_window->xdg_surface, w, h);
    be_window->x = x; be_window->y = y;
    logmsg("set_size serial is %u\n", serial);
}

// request an explicit repaint
void be_repaint(backend_t *be){
}
