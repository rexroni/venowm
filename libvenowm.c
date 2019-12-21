#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include "libvenowm.h"
#include "protocol/venowm-shell-client-protocol.h"

struct venowm {
    bool connected;
    struct wl_display *display;
    bool free_display_in_cleanup;
    struct wl_registry *registry;
    uint32_t global_uid;
    bool failed;
    char errmsg[1024];
    // the wayland interface to the venowm_control protocol
    struct venowm_control *venowm_control;
};

// like snprintf(v->errmsg, sizeof(v->errmsg), fmt, ...), but safe and concise
static inline void errmsg(struct venowm *v, const char *fmt, ...){
    // don't overwrite the first error
    if(v->failed) return;

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(v->errmsg, sizeof(v->errmsg), fmt, ap);
    va_end(ap);

    // forcibly null terminate
    v->errmsg[sizeof(v->errmsg) - 1] = '\0';

    // indicate we have an error
    v->failed = true;
}

static void registry_handle_global(void *data, struct wl_registry *registry,
        uint32_t uid, const char *interface, uint32_t version){
    struct venowm *v = data;

    if(strcmp(interface, "venowm_control") == 0){
        v->venowm_control = wl_registry_bind(registry, uid, &venowm_control_interface, 1);
        v->global_uid = uid;
        v->connected = true;
    }
}

static void registry_handle_global_remove(void *data,
        struct wl_registry *registry, uint32_t uid){
    struct venowm *v = data;

    if(v->global_uid == uid){
        errmsg(v, "venowm global disappeared from registry");
    }
}

static const struct wl_registry_listener registry_listener = {
    registry_handle_global,
    registry_handle_global_remove,
};

struct venowm *venowm_create(){
    struct venowm *v = malloc(sizeof(struct venowm));
    if(v == NULL) return v;
    *v = (struct venowm){0};
    return v;
}

void venowm_destroy(struct venowm *v){
    if(v == NULL) return;
    if(v->venowm_control != NULL) venowm_control_destroy(v->venowm_control);
    if(v->registry != NULL) wl_registry_destroy(v->registry);
    if(v->display != NULL && v->free_display_in_cleanup){
        wl_display_disconnect(v->display);
    }
    free(v);
}

const char *venowm_errmsg(struct venowm *v){
    return v->errmsg;
}

int venowm_connect(struct venowm *v, struct wl_display *wl_display){
    if(v->failed) return -1;
    if(v->connected){
        errmsg(v, "already connected!");
        return -1;
    }

    // do we have to find the display ourselves?
    if(wl_display == NULL){
        // we access the display ourselves
        wl_display = wl_display_connect(NULL);
        if(wl_display == NULL){
            errmsg(v, "failed to connect to display");
            return -1;
        }
        v->free_display_in_cleanup = true;
    }
    v->display = wl_display;

    // get the registry
    v->registry = wl_display_get_registry(v->display);
    if(v->registry == NULL){
        errmsg(v, "failed to get registry from display");
        return -1;
    }

    // add our listener to the registry
    int ret = wl_registry_add_listener(v->registry, &registry_listener, v);
    if(ret < 0){
        errmsg(v, "failed to get add listener to registry");
        return -1;
    }

    // sync with the server, getting all the initial globals
    ret = wl_display_roundtrip(v->display);
    if(ret < 0){
        errmsg(v, "failed to sync with display server");
        return -1;
    }

    // confirm that we found the venowm_control global
    if(!v->connected){
        errmsg(v, "unable to find venowm in wayland registry");
        return -1;
    }

    return 0;
}

int venowm_flush(struct venowm *v){
    if(v->failed) return -1;
    if(!v->connected){
        errmsg(v, "not connected yet!");
        return -1;
    }

    int ret = wl_display_roundtrip(v->display);
    if(ret < 0){
        errmsg(v, "failed to sync with display server");
        return -1;
    }

    return 0;
}


// a type for all of the simple venowm_control commands
typedef void (*venowm_cmd_t)(struct venowm_control*);

static int do_venowm_command(struct venowm *v, bool flush, venowm_cmd_t cmd){
    if(v->failed) return -1;
    if(!v->connected){
        errmsg(v, "not connected yet!");
        return -1;
    }

    cmd(v->venowm_control);

    if(!flush) return 0;

    return venowm_flush(v);
}

int venowm_focus_up(struct venowm *v, bool flush){
    return do_venowm_command(v, flush, venowm_control_focus_up);
}

int venowm_focus_down(struct venowm *v, bool flush){
    return do_venowm_command(v, flush, venowm_control_focus_down);
}

int venowm_focus_left(struct venowm *v, bool flush){
    return do_venowm_command(v, flush, venowm_control_focus_left);
}

int venowm_focus_right(struct venowm *v, bool flush){
    return do_venowm_command(v, flush, venowm_control_focus_right);
}
