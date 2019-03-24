#include <stdio.h>
#include <stdlib.h>

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
    out->win_info = NULL;
    out->screen = parent ? parent->screen : NULL;
    return out;
}

// frees all the split_t objects, closing windows that are left
void split_free(split_t *split){
    if(!split) return;
    // win_info should be empty here.  If it's not, we can't fix it now
    split_free(split->frames[0]);
    split_free(split->frames[1]);
    free(split);
}

// returns 0 for OK, -1 for error
/* first child inherits any window or global focus, but redrawing that window
   has to be done at a higher level.  Same with uncovering a hidden window */
int split_do_split(split_t *split, bool vertical, float fraction){
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
    // if we have a win_info that needs to be passed to the child
    if(split->win_info){
        // move *win_info to child
        split->frames[0]->win_info = split->win_info;
        split->win_info = NULL;
        // fix *frame pointer in win_info
        split->frames[0]->win_info->frame = split->frames[0];
    }
    // pass focus if necessary
    if(g_workspace->focus == split){
        g_workspace->focus = split->frames[0];
    }

    return 0;
}

/* Workspace should pre-check and not call this on a root frame.  The window in
   this frame should already have been hidden.  Redrawing and fixing focus has
   to be done at a higher level.  The return value is the next frame
   that would be focused on (though moving focus is not done here). */
split_t *split_do_remove(split_t *split){
    split_t *parent = split->parent;
    split_t *other = parent->frames[split == parent->frames[0]];
    // parent inherits all the goodness that was the other child frame
    parent->frames[0] = other->frames[0];
    parent->frames[1] = other->frames[1];
    parent->isvertical = other->isvertical;
    parent->fraction = other->fraction;
    parent->isleaf = other->isleaf;
    parent->win_info = other->win_info;
    // fix backrefs that used to point to other child
    if(parent->win_info) parent->win_info->frame = parent;
    if(parent->frames[0]) parent->frames[0]->parent = parent;
    if(parent->frames[1]) parent->frames[1]->parent = parent;
    // now free the other child
    other->frames[0] = NULL;
    other->frames[1] = NULL;
    split_free(other);
    // now free fre frame we are removing
    split_free(split);
    // find the firstmost leaf of the parent frame
    split_t *remains = parent;
    while(!remains->isleaf) remains = remains->frames[0];
    return remains;
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

split_t *do_split_move(split_t *start, bool vertical, bool increasing){
    /* pos is the centerpoint of the starting frame on the border-to-cross
       relative to our current position */
    float pos = 0.5;
    // go up the tree until we find the split we want to cross
    split_t *here = start;
    while(true){
        split_t *parent = here->parent;
        if(!parent){
            // found root split
            // TODO: handle multiple screens
            return start;
        }
        bool first_child = (parent->frames[0] == here);
        if(parent->isvertical != vertical){
            // split is the wrong way, recalculate pos
            if(first_child){
                pos *= parent->fraction;
            }else{
                pos = parent->fraction + (1-parent->fraction)*pos;
            }
        }else if(first_child == increasing){
            // we found the split we need to cross
            here = here->parent;
            break;
        }else{
            // split is the right way, but we are on the wrong side of it
            // (continue up tree, nothing to do here)
        }
        here = here->parent;
    }
    /* now *here points to the parent of the split we want to cross.  First,
       just cross that split. */
    here = here->frames[increasing];

    // Now descend to the appropriate leaf.
    while(!here->isleaf){
        if(here->isvertical == vertical){
            // easy case, no need to recalculate pos
            // if we are increasing, take the first child
            // (if we are going *right*, take the *left* child)
            // ((damn this is confusing))
            here = here->frames[increasing == 0];
        }else{
            // ties go to first child
            if(pos <= here->fraction){
                // take the first child
                pos = pos / here->fraction;
                here = here->frames[0];
            }else{
                // take the second child
                pos = (pos - here->fraction) / (1 - here->fraction);
                here = here->frames[1];
            }
        }
    }
    // done! *here points to where we want to be
    return here;
}

static int do_at_each(split_t *split, split_do_cb_t cb, void* data,
                      float t, float b, float l, float r){
    int ret = 0;
    ret = cb(split, data, t, b, l, r);
    if(ret) return ret;
    // don't descend past a leaf
    if(split->isleaf) return 0;
    if(split->isvertical){
        float line = t + (b-t)*split->fraction;
        ret = do_at_each(split->frames[0], cb, data, t, line, l, r);
        if(ret) return ret;
        ret = do_at_each(split->frames[1], cb, data, line, b, l, r);
        if(ret) return ret;
    }else{
        float line = l + (r-l)*split->fraction;
        ret = do_at_each(split->frames[0], cb, data, t, b, l, line);
        if(ret) return ret;
        ret = do_at_each(split->frames[1], cb, data, t, b, line, r);
        if(ret) return ret;
    }
    return 0;
}

int split_do_at_each(split_t *split, split_do_cb_t cb, void* data){
    return do_at_each(split, cb, data, 0.0, 1.0, 0.0, 1.0);
}
