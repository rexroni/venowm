#ifndef BACKEND_H
#define BACKEND_H

#include <stdint.h>
#include <time.h>

#if !defined(USE_SWC) && !defined(USE_WESTON)
    #error "you must define USE_SWC or USE_WESTON"
#endif

/* don't like the #define strategy, there is no way to say that a
   struct of one name just refers to a struct of another name. */

#if defined(USE_SWC)
    #include <swc.h>
    typedef struct swc_screen be_screen_t;
    typedef struct swc_window be_window_t;
#elif defined(USE_WESTON)
    #include <compositor.h>
    typedef struct be_screen_t be_screen_t;
    typedef struct be_window_t be_window_t;
#endif

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

/* don't like the #define strategy, but since there is no way in the callback
   to see what the modifier was on the key, it is not possible to have a single
   callback in the backend that dispatches other callbacks without some
   low-level hacking on the libraries underneath.  Therefore, the best strategy
   is to use macros to abstract the various arguments away. */

#if defined(USE_SWC)

    typedef void (*be_key_handler_t)(void*, uint32_t, uint32_t, uint32_t);

    #define DEFINE_KEY_HANDLER(func_name) \
        void func_name(void *data, uint32_t time, uint32_t value, \
                       uint32_t state){ \
            (void)time; (void)value; (void)state; (void)data; \
            if(state != WL_KEYBOARD_KEY_STATE_PRESSED) return;

    #define FINISH_KEY_HANDLER \
        }

#elif defined(USE_WESTON)

    typedef void (*be_key_handler_t)(struct weston_keyboard*,
                                     const struct timespec*,
                                     uint32_t,
                                     void*);

    #define DEFINE_KEY_HANDLER(func_name) \
        void func_name(struct weston_keyboard *keyboard, \
                       const struct timespec *timespec, \
                       uint32_t value, \
                       void *data){ \
            (void)keyboard; (void)timespec; (void)value; (void)data;

    #define FINISH_KEY_HANDLER \
        }

#endif

int be_handle_key(backend_t *be, uint32_t mods, uint32_t key,
                  be_key_handler_t handler, void *data);

void be_screen_get_geometry(be_screen_t *be_screen,
                            int32_t *x, int32_t *y, uint32_t *w, uint32_t *h);

void be_window_focus(be_window_t *be_window);
void be_window_hide(be_window_t *be_window);
void be_window_show(be_window_t *be_window, be_screen_t *be_screen);
void be_window_close(be_window_t *be_window);
void be_window_geometry(be_window_t *be_window,
                        int32_t x, int32_t y, uint32_t w, uint32_t h);

//// CALLBACKS
// (not defined in backend.c and must be defined elsewhere)

/* handle_*_new fuctions set a void* which gets returned by other callbacks,
   and return 0 if everything went well */
int handle_screen_new(be_screen_t *be_screen, void **data);
void handle_screen_geometry(void *data);
void handle_screen_destroy(void *data);

int handle_window_new(be_window_t *be_window, void **data);
void handle_window_destroy(void *data);

#endif // BACKEND_H
