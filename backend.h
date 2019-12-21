#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>
#include <time.h>

typedef struct be_screen_t be_screen_t;
typedef struct be_window_t be_window_t;

struct backend_t;
typedef struct backend_t backend_t;

backend_t *backend_new(void);
int backend_run(backend_t *be);
void backend_stop(backend_t *be);
void backend_free(backend_t *be);

enum {
    MOD_CTRL = 1 << 0,
    MOD_ALT = 1 << 1,
    MOD_SUPER = 1 << 2,
    MOD_SHIFT = 1 << 3,
};

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
        bool (*func)(backend_t*, void*), void *data);

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h);

void be_unfocus_all(backend_t *be);
void be_window_focus(be_window_t *be_window);
void be_window_hide(be_window_t *be_window);
void be_window_show(be_window_t *be_window, be_screen_t *be_screen);
void be_window_close(be_window_t *be_window);
void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h);

// request an explicit repaint
void be_repaint(backend_t *be);

//// CALLBACKS TO REST OF THE SYSTEM
// (not defined in backend.c and must be defined elsewhere)

/* handle_*_new fuctions set a void* which gets returned by other callbacks,
   and return 0 if everything went well */
int handle_screen_new(be_screen_t *be_screen, void **data);
void handle_screen_geometry(void *data);
void handle_screen_destroy(void *data);

int handle_window_new(be_window_t *be_window, void **data);
void handle_window_destroy(void *data);

#endif // BACKEND_H
