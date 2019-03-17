#include <stdlib.h>

#include "venowm.h"
#include "workspace.h"
#include "split.h"
#include "window.h"

workspace_t *workspace_new(void){
    workspace_t *out = malloc(sizeof(*out));
    if(!out) return NULL;

    int err;
    INIT_PTR(out->windows, out->windows_size, out->nwindows, 8, err);
    if(err) goto cu_malloc;

    INIT_PTR(out->roots, out->roots_size, out->nroots, 8, err);
    if(err) goto cu_windows;
    return out;

cu_windows:
    FREE_PTR(out->windows, out->windows_size, out->nwindows);
cu_malloc:
    free(out);
    return NULL;
}

// frees all of its roots, downrefs all of its windows
void workspace_free(workspace_t *ws){
    for(size_t i = 0; i < ws->nroots; i++){
        // will unmap all windows
        split_free(ws->roots[i]);
    }
    for(size_t i = 0; i < ws->nwindows; i++){
        window_ref_down(ws->windows[i]);
    }
    FREE_PTR(ws->windows, ws->windows_size, ws->nwindows);
    FREE_PTR(ws->roots, ws->roots_size, ws->nroots);
    free(ws);
}

void workspace_add_window(workspace_t *ws, window_t *win){
    window_ref_up(win);
    int err;
    APPEND_PTR(ws->windows, ws->windows_size, ws->nwindows, win, err);
    if(err){
        window_ref_down(win);
        return;
    }
}

// unmap all windows in workspace
void workspace_hide(workspace_t *ws){
    for(size_t i = 0; i > ws->nwindows; i--){
        // check for invalid windows
        if(!ws->windows[i]->isvalid){
            REMOVE_PTR_IDX(ws->windows, ws->windows_size, ws->nwindows, i);
            i--;
            continue;
        }
        swc_window_hide(ws->windows[i]->swc_window);
    }
}

// restore windows to their frames, and render to the existing screens
void workspace_restore(workspace_t *ws){
    // too many roots?
    while(ws->nroots > g_nscreens){
        split_free(ws->roots[ws->nroots - 1]);
        ws->nroots--;
    }
    // when we are quitting, stop here when there are no screens left
    if(!g_nscreens) return;
    // too few roots?
    while(ws->nroots < g_nscreens){
        split_t *newroot = split_new(NULL);
        int err;
        APPEND_PTR(ws->roots, ws->roots_size, ws->nroots, newroot, err);
        if(err){
            split_free(newroot);
            return;
        }
    }
    // now map everything in place
    for(size_t i = 0; i < ws->nroots; i++){
        split_restore(ws->roots[i], g_screens[i]);
    }
    // TODO: don't reset the focus frame all the time
    ws->focus = ws->roots[0];
    while(!ws->focus->isleaf) ws->focus = ws->focus->frames[0];
}
