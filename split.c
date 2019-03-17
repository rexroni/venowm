#include <stdio.h>
#include <stdlib.h>
#include <swc.h>

#include "split.h"
#include "window.h"

split_t *split_new(split_t *parent){
    split_t *out = malloc(sizeof(*out));
    if(!out) return NULL;
    out->isvertical = false;
    out->fraction = 1.0;
    out->parent = parent;
    out->frames[0] = NULL;
    out->frames[1] = NULL;
    out->isleaf = true;
    out->window = NULL;
    return out;
}

// frees all the split_t objects and unmaps/downrefs all windows
void split_free(split_t *split){
    if(!split) return;
    if(split->window){
        swc_window_hide(split->window->swc_window);
        window_ref_down(split->window);
    }
    split_free(split->frames[0]);
    split_free(split->frames[1]);
    free(split);
}

// returns 0 for OK, -1 for error
int do_split(split_t *split, bool vertical, float fraction){
    // allocate two children
    split->frames[0] = split_new(split);
    if(!split->frames[0]) return -1;
    split->frames[1] = split_new(split);
    if(!split->frames[1]){
        split_free(split->frames[0]);
        return -1;
    }
    // set values
    split->fraction = fraction;
    split->isvertical = vertical;
    split->isleaf = false;
    return 0;
}

sides_t get_sides(split_t *split){
    // fractions of the view so far
    float t = 0.0, b = 1.0, l = 0.0, r = 1.0;
    for(split_t *cur = split; cur->parent; cur = cur->parent){
        // which child are we?
        int idx = (cur->parent->frames[1] == cur);
        // parent's fraction
        float pfrac = cur->parent->fraction;
        if(cur->parent->isvertical){
            // vertical split
            if(idx == 0){
                // first child
                t *= pfrac;
                b *= pfrac;
            }else{
                t = pfrac + (1-pfrac)*t;
                b = pfrac + (1-pfrac)*b;
            }
        }else{
            // horizontal split
            if(idx == 0){
                // first child
                l *= pfrac;
                r *= pfrac;
            }else{
                l = pfrac + (1-pfrac)*l;
                r = pfrac + (1-pfrac)*r;
            }
        }
    }
    return (sides_t){.t = t, .b = b, .l = l, .r = r};
}

static void do_show_window(window_t *window, screen_t *screen,
                           float t, float b, float l, float r){
    swc_window_show(window->swc_window);
    // pull out screen geometry
    int32_t x = screen->swc_screen->usable_geometry.x;
    int32_t y = screen->swc_screen->usable_geometry.y;
    uint32_t w = screen->swc_screen->usable_geometry.width;
    uint32_t h = screen->swc_screen->usable_geometry.height;
    // build window geometry
    // TODO: decide how to do the offsets to avoid skipping/overlapping pixels
    int32_t xmin = x + frac_of(l, (int)w);
    int32_t xmax = x + frac_of(r, (int)w);
    int32_t ymin = y + frac_of(t, (int)h);
    int32_t ymax = y + frac_of(b, (int)h);
    uint32_t wout = xmax - xmin;
    uint32_t hout = ymax - ymin;
    // set window geometry
    swc_window_set_position(window->swc_window, xmin, ymin);
    swc_window_set_size(window->swc_window, wout, hout);
}

static void do_split_restore(split_t *split, screen_t *screen,
                             float t, float b, float l, float r){
    if(split->isleaf){
        if(split->window){
            // check validity of window
            if(!split->window->isvalid){
                window_ref_down(split->window);
                split->window = NULL;
            }else{
                do_show_window(split->window, screen, t, b, l, r);
            }
        }
        return;
    }
    if(split->isvertical){
        float line = t + (b-t)*split->fraction;
        do_split_restore(split->frames[0], screen, t, line, l, r);
        do_split_restore(split->frames[1], screen, line, b, l, r);
    }else{
        float line = l + (r-l)*split->fraction;
        do_split_restore(split->frames[0], screen, t, b, l, line);
        do_split_restore(split->frames[1], screen, t, b, line, r);
    }
}

// call window_map on all frames, *split must be a root
void split_restore(split_t *split, screen_t *screen){
    if(split->parent){
        logmsg("whoa there, you can't restore that split!\n");
    }
    do_split_restore(split, screen, 0.0, 1.0, 0.0, 1.0);
}

// call window_map on a single frame, *split must not be a leaf
void split_map_window(split_t *split, screen_t *screen, window_t *window){
    if(!split->isleaf){
        logmsg("whoa there, you can't stick that window there!\n");
    }
    // check window validity
    if(!window->isvalid){
        window_ref_down(window);
        return;
    }
    // are we unmapping a window already in that split?
    if(split->window){
        window_ref_down(split->window);
        split->window = NULL;
    }
    // save the window in the split
    window_ref_up(window);
    split->window = window;
    // get the geometry of the window
    sides_t sides = get_sides(split);
    float t = sides.t, b = sides.b, l = sides.l, r = sides.r;
    do_show_window(window, screen, t, b, l, r);
}

