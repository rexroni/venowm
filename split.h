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
// frees all the split_t objects and unmaps/downrefs all windows
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

// returns 0 for OK, -1 for error
int do_split(split_t *split, bool vertical, float fraction);

static inline int vsplit(split_t *split, float fraction){
    return do_split(split, true, fraction);
}

static inline int hsplit(split_t *split, float fraction){
    return do_split(split, false, fraction);
}

// call window_map on all frames's windows, *split must be a root
void split_restore(split_t *split, screen_t *screen);

// call window_map on a single frame, *split must be a leaf
void split_map_window(split_t *split, window_t *window);

// returns NULL if no move is possible (such as an edge)
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

#endif // SPLIT_H
