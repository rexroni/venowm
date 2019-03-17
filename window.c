#include <stdlib.h>
#include <swc.h>

#include "window.h"
#include "workspace.h"
#include "split.h"

void handle_window_destroy(void *data){
    logmsg("destroy window\n");
    // dereference window_t
    window_t *window = data;
    window->isvalid = false;
    window_ref_down(window);
}

static struct swc_window_handler window_handler = {
    .destroy = &handle_window_destroy,
    .title_changed = NULL,
    .app_id_changed = NULL,
    .parent_changed = NULL,
    .entered = NULL,
    .move = NULL,
    .resize = NULL,
};

void handle_new_window(struct swc_window *swc_window){
    logmsg("new window\n");
    // wrap this window in a new window_t
    window_t *window = window_new(swc_window);
    if(!window){
        /* oops, no memory.  Close the window.  No handler has been set, so we
           don't need to worry about receiving a window.destroy hook */
        logmsg("no memory! closing window");
        swc_window_close(swc_window);
        return;
    }
    // append this window to the current workspace
    int err;
    workspace_t *ws = g_workspace;
    APPEND_PTR(ws->windows, ws->windows_size, ws->nwindows, window, err);
    if(err){
        logmsg("no memory! closing window");
        // one downref will cause the window to close
        window_ref_down(window);
        return;
    }
    // add window to the current workspace
    workspace_add_window(g_workspace, window);
    // set some defaults
    swc_window_set_border(window->swc_window, 0, 0);
    // give the window focus
    swc_window_focus(window->swc_window);
    // map the window to the current frame
    split_map_window(g_workspace->focus, window);
}

// the returned window starts with 2 refs: this ptr and the swc callback
window_t *window_new(struct swc_window *swc_window){
    window_t *out = malloc(sizeof(*out));
    if(out == NULL){
        return NULL;
    }
    // store swc pointer
    out->swc_window = swc_window;
    // set the callback
    swc_window_set_handler(swc_window, &window_handler, out);
    // one pointer is the returned *out, the other is the swc callback
    out->refs = 2;
    out->isvalid = true;
    return out;
}

void window_ref_up(window_t *window){
    window->refs++;
}

void window_ref_down(window_t *window){
    window->refs--;
    if(window->refs == 1 && window->isvalid){
        // the last reference is the app callback; close the window
        swc_window_close(window->swc_window);
    }else if(window->refs == 0){
        // free the window struct
        free(window);
    }
}

