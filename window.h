#ifndef WINDOW_H
#define WINDOW_H

#include <swc.h>

#include "venowm.h"

// the returned window starts with 0 refs
window_t *window_new(be_window_t *be_window);

void window_ref_up(window_t *window);
void window_ref_down(window_t *window);

void window_map(window_t *window, split_t *frame);
void window_unmap(window_t *window);

#endif // WINDOW_H
