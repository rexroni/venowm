#ifndef WINDOW_H
#define WINDOW_H

#include <swc.h>

#include "venowm.h"

// the returned window starts with 2 refs: this ptr and the swc callback
window_t *window_new(struct swc_window *swc_window);

void window_ref_up(window_t *window);
void window_ref_down(window_t *window);

void window_map(window_t *window, split_t *frame);
void window_unmap(window_t *window);

void handle_new_window(struct swc_window *swc_window);

#endif // WINDOW_H
