#ifndef SPLIT_H
#define SPLIT_H

#include <math.h>

#include "venowm.h"

// takes a size S and returns within [0,(S-1)]
static inline int frac_of(float f, int size){
    return (int)roundf(f * (size-1));
}

// use parent=NULL for a root element
split_t *split_new(split_t *parent);
// frees all the split_t objects, closing windows that are left
void split_free(split_t *split);

typedef struct {
    float t;
    float b;
    float l;
    float r;
} sides_t;

/* Get the top, bottom, left, and right boundaires (as a fraction of total
   screen area) for a given split. */
sides_t get_sides(split_t *split);

// returns 0 for OK or -1 for error
/* first child inherits any window or global focus, but redrawing that window
   has to be done at a higher level.  Same with uncovering a hidden window */
int split_do_split(split_t *split, bool vertical, float fraction);

/* Workspace should pre-check and not call this on a root frame.  The window in
   this frame should already have been hidden.  Parent split will inherit focus
   from this child or a window from the other.  Redrawing any window has to be
   done at a higher level. */
void split_do_remove(split_t *split);

// call window_map on a single frame, *split must be a leaf
void split_map_window(split_t *split, ws_win_info_t *win_info);

// never returns NULL; if no move possible, returns *start
split_t *do_split_move(split_t *start, bool vertical, bool increasing);

static inline split_t *split_move_right(split_t *start){
    return do_split_move(start, false, true);
}
static inline split_t *split_move_left(split_t *start){
    return do_split_move(start, false, false);
}
static inline split_t *split_move_up(split_t *start){
    return do_split_move(start, true, false);
}
static inline split_t *split_move_down(split_t *start){
    return do_split_move(start, true, true);
}

// get a callback at every leaf of the split tree, with relative boundaries
typedef int (*split_do_cb_t)(split_t *split, void* data,
                             float t, float b, float l, float r);

int split_do_at_each(split_t *split, split_do_cb_t cb, void* data);

#endif // SPLIT_H
