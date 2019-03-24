#include <stdlib.h>

#include "window.h"
#include "workspace.h"
#include "split.h"

int handle_window_new(be_window_t *be_window, void **data){
    logmsg("new window\n");
    // wrap this window in a new window_t
    window_t *window = window_new(be_window);
    if(!window){
        /* oops, no memory.  Close the window.  No handler will been set, so we
           don't need to worry about receiving a window.destroy hook */
        logmsg("no memory! closing window");
        be_window_close(be_window);
        return -1;
    }
    // add window to the current workspace, mapping/focusing it immediately
    workspace_add_window(g_workspace, window, true);
    // set callback data
    *data = window;
    return 0;
}

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

// the returned window starts with 0 refs
window_t *window_new(be_window_t *be_window){
    window_t *out = malloc(sizeof(*out));
    if(out == NULL){
        return NULL;
    }
    // not drawn yet
    out->screen = NULL;
    // store be_window pointer
    out->be_window = be_window;
    // set defaults
    out->refs = 0;
    out->isvalid = true;
    return out;
}

void window_ref_up(window_t *window){
    window->refs++;
}

void window_ref_down(window_t *window){
    if(--window->refs < 1){
        // make sure the be_window has not already closed
        if(window->isvalid){
            // close the window
            be_window_close(window->be_window);
        }
    }
}

