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
    // go through each workspace and remove this window
    for(size_t i = 0; i < g_nworkspaces; i++){
        workspace_remove_window(g_workspaces[i], window);
    }
    // no more references to window, free it
    free(window);
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
    // set some defaults
    swc_window_set_border(window->swc_window, 0, 0);
    swc_window_set_tiled(window->swc_window);
    // add window to the current workspace, mapping/focusing it immediately
    workspace_add_window(g_workspace, window, true);
}

// the returned window starts with 0 refs
window_t *window_new(struct swc_window *swc_window){
    window_t *out = malloc(sizeof(*out));
    if(out == NULL){
        return NULL;
    }
    // not drawn yet
    out->screen = NULL;
    // store swc pointer
    out->swc_window = swc_window;
    // set the callback
    swc_window_set_handler(swc_window, &window_handler, out);
    // one pointer is the returned *out, the other is the swc callback
    out->refs = 0;
    out->isvalid = true;
    return out;
}

void window_ref_up(window_t *window){
    window->refs++;
}

void window_ref_down(window_t *window){
    if(--window->refs < 1){
        // make sure the swc_window has not already closed
        if(window->isvalid){
            // close the window
            swc_window_close(window->swc_window);
        }
    }
}

