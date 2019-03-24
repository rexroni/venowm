#include <stdlib.h>

#include "venowm.h"
#include "workspace.h"
#include "split.h"
#include "window.h"

workspace_t *workspace_new(void){
    workspace_t *out = malloc(sizeof(*out));
    if(!out) return NULL;

    out->windows = kh_init(wswl);
    if(!out) goto cu_malloc;

    // no hidden windows yet
    out->hidden_first = NULL;
    out->hidden_last = NULL;

    int err;
    INIT_PTR(out->roots, out->roots_size, out->nroots, 8, err);
    if(err) goto cu_windows;
    return out;

cu_windows:
    kh_destroy(wswl, out->windows);
cu_malloc:
    free(out);
    return NULL;
}

static void hidden_prepend(workspace_t *ws, ws_win_info_t *info){
    // if list is empty, info becomes first and last
    if(ws->hidden_first == NULL){
        ws->hidden_first = info;
        ws->hidden_last = info;
        info->prev = NULL;
        info->next = NULL;
        return;
    }
    // otherwise just prepend
    info->next = ws->hidden_first;
    ws->hidden_first->prev = info;
    info->prev = NULL;
    ws->hidden_first = info;
}

static void hidden_append(workspace_t *ws, ws_win_info_t *info){
    // if list is empty, info becomes first and last
    if(ws->hidden_last == NULL){
        ws->hidden_first = info;
        ws->hidden_last = info;
        info->prev = NULL;
        info->next = NULL;
        return;
    }
    // otherwise just append
    info->prev = ws->hidden_last;
    ws->hidden_last->next = info;
    info->next = NULL;
    ws->hidden_last = info;
}

static ws_win_info_t *hidden_pop_first(workspace_t *ws){
    // check if list is empty
    if(ws->hidden_first == NULL) return NULL;
    ws_win_info_t *out = ws->hidden_first;
    // check if there is only one element
    if(ws->hidden_first == ws->hidden_last){
        ws->hidden_first = NULL;
        ws->hidden_last = NULL;
    }else{
        out->next->prev = NULL;
        ws->hidden_first = out->next;
    }
    out->prev = NULL;
    out->next = NULL;
    return out;
}

static ws_win_info_t *hidden_pop_last(workspace_t *ws){
    // check if list is empty
    if(ws->hidden_last == NULL) return NULL;
    ws_win_info_t *out = ws->hidden_last;
    // check if there is only one element
    if(ws->hidden_first == ws->hidden_last){
        ws->hidden_first = NULL;
        ws->hidden_last = NULL;
    }else{
        out->prev->next = NULL;
        ws->hidden_last = out->prev;
    }
    out->prev = NULL;
    out->next = NULL;
    return out;
}

static void hidden_remove(workspace_t *ws, ws_win_info_t *info){
    // was it the first element?
    if(ws->hidden_first == info)
        ws->hidden_first = info->next;
    // was it the last element?
    if(ws->hidden_last == info)
        ws->hidden_last = info->prev;
    // had prev?
    if(info->prev)
        info->prev->next = info->next;
    // had next?
    if(info->next)
        info->next->prev = info->prev;
    info->prev = NULL;
    info->next = NULL;
}

// frees all of its roots, downrefs all of its windows
void workspace_free(workspace_t *ws){
    // iterate through all windows via hashmap
    khiter_t k;
	for (k = kh_begin(ws->windows); k != kh_end(ws->windows); ++k){
		if (kh_exist(ws->windows, k)){
            ws_win_info_t *info = kh_value(ws->windows, k);
            // remove from hashmap
	        kh_del(wswl, ws->windows, k);
            // remove from frame
            workspace_remove_window_from_frame(ws, info->frame, false);
            // remove from hidden list
            hidden_remove(ws, info);
            // no more references from this workspace
            window_ref_down(info->window);
            free(info);
        }
    }
    kh_destroy(wswl, ws->windows);
    // now free all of the ws->nroots
    for(size_t i = 0; i < ws->nroots; i++){
        split_free(ws->roots[i]);
    }
    FREE_PTR(ws->roots, ws->roots_size, ws->nroots);
    free(ws);
}

static void redraw_frame(split_t *frame, screen_t *screen,
                         float t, float b, float l, float r){
    // pull out screen geometry
    int32_t x, y;
    uint32_t w, h;
    be_screen_get_geometry(screen->be_screen, &x, &y, &w, &h);
    // build window geometry
    // TODO: decide how to do the offsets to avoid skipping/overlapping pixels
    int32_t xmin = x + frac_of(l, (int)w);
    int32_t xmax = x + frac_of(r, (int)w);
    int32_t ymin = y + frac_of(t, (int)h);
    int32_t ymax = y + frac_of(b, (int)h);
    uint32_t wout = xmax - xmin;
    uint32_t hout = ymax - ymin;
    // make window visible and set the geometry
    be_window_t *be_window = frame->win_info->window->be_window;
    be_window_show(be_window);
    be_window_geometry(be_window, xmin, ymin, wout, hout);
}

static void draw_window(ws_win_info_t *info, split_t *frame){
    // "draw window in NULL" -> noop
    if(!frame) return;
    // "draw NULL in frame" -> erase win_info in that frame
    frame->win_info = info;
    if(!info) return;
    info->frame = frame;
    // don't actually redraw the window unless it is on screen
    if(!frame->screen) return;
    // get the geometry of the window
    sides_t sides = get_sides(frame);
    float t = sides.t, b = sides.b, l = sides.l, r = sides.r;
    // draw the window
    redraw_frame(frame, frame->screen, t, b, l, r);
}

void workspace_add_window(workspace_t *ws, window_t *window, bool map_now){
    window_ref_up(window);

    // allocate/init win_info struct
    ws_win_info_t *info = malloc(sizeof(*info));
    if(!info){
        window_ref_down(window);
        return;
    }
    *info = (ws_win_info_t){.window = window};

    // pack info into hash table
    khint_t k;
    int ret;
    // get index
    k = kh_put(wswl, ws->windows, window, &ret);
    if(ret < 0){
        window_ref_down(window);
        free(info);
        return;
    }
    // write to index
    kh_value(ws->windows, k) = info;

    if(map_now){
        // hide whatever window is currently in the focused frame
        workspace_remove_window_from_frame(ws, ws->focus, false);
        // draw this window
        draw_window(info, ws->focus);
        // give the window focus
        workspace_focus_frame(ws, ws->focus);
    }else{
        // append window to hidden windows
        hidden_append(ws, info);
    }
}

// removes a window from the workspace and downrefs it
void workspace_remove_window(workspace_t *ws, window_t *window){
    // get backreferences to window via win_info struct
    khiter_t k;
	k = kh_get(wswl, ws->windows, window);
    if(k == kh_end(ws->windows)){
        // this window isn't part of this workspace
        return;
    }
	ws_win_info_t *info = kh_value(ws->windows, k);
	kh_del(wswl, ws->windows, k);
    split_t *frame = info->frame;
    // remove from frame
    workspace_remove_window_from_frame(ws, info->frame, false);
    // remove from hidden list
    hidden_remove(ws, info);
    // replace with another window
    workspace_next_hidden_win_at(ws, frame);
    // no more references from this workspace
    window_ref_down(info->window);
    free(info);
}

void workspace_remove_window_from_frame(workspace_t *ws, split_t *split,
                                        bool prepend_old_window){
    if(!split) return;
    ws_win_info_t *info = split->win_info;
    if(!info) return;
    // hide window if workspace is active
    if(g_workspace == ws){
        be_window_hide(info->window->be_window);
    }
    split->win_info = NULL;
    info->frame = NULL;
    if(prepend_old_window)
        hidden_prepend(ws, info);
    else
        hidden_append(ws, info);
}

static int hide_cb(split_t *split, void *data,
                   float t, float b, float l, float r){
    (void)data; (void)t; (void)b; (void)l; (void)r;
    // split is no longer associated with a screen
    split->screen = NULL;
    // that's all we do for non-leaves
    if(!split->isleaf) return 0;
    // do nothing if this leaf has no window
    if(!split->win_info) return 0;
    // hide window
    swc_window_hide(split->win_info->window->be_window);
    return 0;
}

// unmap all windows in workspace
void workspace_hide(workspace_t *ws){
    for(size_t i = 0; i < ws->nroots; i++){
        split_do_at_each(ws->roots[i], hide_cb, NULL);
    }
}

static int restore_cb(split_t *split, void *data,
                      float t, float b, float l, float r){
    // dereference screen
    screen_t *screen = data;
    // save screen
    split->screen = screen;
    // that's all we do for non-leaves
    if(!split->isleaf) return 0;
    // do nothing if this leaf has no window
    if(!split->win_info) return 0;
    // draw the window
    redraw_frame(split, screen, t, b, l, r);
    return 0;
}

static int pre_rm_root_cb(split_t *split, void *data,
                          float t, float b, float l, float r){
    (void)t; (void)b; (void)l; (void)r;
    // dereference workspace
    workspace_t *ws = data;
    // remove any window in the frame (and list it as hidden)
    workspace_remove_window_from_frame(ws, split, false);
    return 0;
}


// restore windows to their frames, and render to the existing screens
void workspace_restore(workspace_t *ws){
    /* TODO: think of a better way to do weakly-persistent root-screen mappings
             (this strategy is OK only in the trivial case (no changes) and
             it is at least safe in more complex cases) */

    // Step 1: too many roots?
    while(ws->nroots > g_nscreens){
        // pull out one root
        split_t *root = ws->roots[ws->nroots - 1];
        ws->nroots--;
        // remove windows from frame and list them as hidden
        split_do_at_each(root, pre_rm_root_cb, ws);
        split_free(root);
    }

    /* if this is called during the screen_destroy handler, it's possible that
       we are about to exit and there are no screens left.  Stop here. */
    if(!g_nscreens) return;

    // Step 2: too few roots?
    while(ws->nroots < g_nscreens){
        split_t *newroot = split_new(NULL);
        int err;
        APPEND_PTR(ws->roots, ws->roots_size, ws->nroots, newroot, err);
        if(err){
            logmsg("no memory to restore workspace\n");
            // just don't draw on that screen I guess
            break;
        }
    }

    // Step 3:  now map everything in place
    for(size_t i = 0; i < ws->nroots; i++){
        split_do_at_each(ws->roots[i], restore_cb, g_screens[i]);
    }

    // TODO: don't reset the focus frame all the time
    ws->focus = ws->roots[0];
    while(!ws->focus->isleaf) ws->focus = ws->focus->frames[0];
}

// trigger workspace to update window focus
void workspace_focus_frame(workspace_t *ws, split_t *frame){
    // store this frame as the focus of the workspace
    ws->focus = frame;
    // focus on the window if this workspace is active
    if(g_workspace == ws){
        if(frame->win_info){
            be_window_focus(frame->win_info->window->be_window);
        }else{
            be_window_focus(NULL);
        }
    }
}

static void workspace_do_split(workspace_t *ws, split_t *split, bool vertical,
                               float fraction){
    // first child inherits whatever was in the old split (window, focus)
    int ret = split_do_split(split, vertical, fraction);
    if(ret) return;
    // redraw window if there was one
    draw_window(split->frames[0]->win_info, split->frames[0]);
    // second child gets a window if one was hidden
    draw_window(hidden_pop_first(ws), split->frames[1]);
}

void workspace_vsplit(workspace_t *ws, split_t *split, float fraction){
    workspace_do_split(ws, split, true, fraction);
}
void workspace_hsplit(workspace_t *ws, split_t *split, float fraction){
    workspace_do_split(ws, split, false, fraction);

}

void workspace_remove_frame(workspace_t *ws, split_t *split){
    // don't do this to root frames
    if(!split->parent) return;
    // remove any window
    workspace_remove_window_from_frame(ws, split, false);
    // remove the frame
    split_t *remains = split_do_remove(split);
    // fix focus if necessary
    if(ws->focus == split){
        workspace_focus_frame(ws, remains);
    }
    if(g_workspace == ws){
        // rerender workspace (recursive redraw necessary in some cases)
        logmsg("rerender!\n");
        workspace_rerender(ws);
    }
}

void workspace_swap_windows_from_frames(split_t *src, split_t *dst){
    if(!src || !dst || src == dst) return;
    ws_win_info_t *src_info = src->win_info;
    ws_win_info_t *dst_info = dst->win_info;
    // place the windows in their new frames
    draw_window(src_info, dst);
    draw_window(dst_info, src);
    // dst might inherit focus from src
    if(g_workspace->focus == src) g_workspace->focus = dst;
}

void workspace_next_hidden_win_at(workspace_t *ws, split_t *split){
    if(!split) return;
    ws_win_info_t *info = hidden_pop_first(ws);
    if(!info) return;
    // remove any window
    workspace_remove_window_from_frame(ws, split, false);
    // place new window
    draw_window(info, split);
    workspace_focus_frame(ws, split);
}

void workspace_prev_hidden_win_at(workspace_t *ws, split_t *split){
    if(!split) return;
    ws_win_info_t *info = hidden_pop_last(ws);
    if(!info) return;
    // remove any window
    workspace_remove_window_from_frame(ws, split, true);
    // place new window
    draw_window(info, split);
    workspace_focus_frame(ws, split);
}
