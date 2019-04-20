#ifndef VENOWM_H
#define VENOWM_H

#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "backend.h"
#include "khash.h"

#include "logmsg.h"

// the main data types.  Some are interdependent.

typedef struct {
    be_screen_t *be_screen;
    // more data to come at a later time
} screen_t;

typedef struct {
    int refs; // how many workspaces is this window in?
    be_window_t *be_window;
    // windows may close or die while there are still open refs to this struct
    bool isvalid;
    // pointer to the screen the window is drawn on (or NULL if not drawn)
    screen_t *screen;
    // pointer to backend
    backend_t *be;
} window_t;

// forward declaration of the special workspace hashtable
struct ws_win_info_t;
typedef struct ws_win_info_t ws_win_info_t;


// define our custom hash table type for "workspace window list" data
// name=wswl
// key type = workspace_t*
// value type = ws_win_info_t
// is_map = true (not a set)
// hash and equality functions are architecture-dependent
#if UINTPTR_MAX == 0xffffffffffffffffULL
    // 64-bit
    #define ptr_hash_func(ptr) kh_int64_hash_func((intptr_t)ptr)
    #define ptr_equal_func(a, b) kh_int64_hash_equal(a, b)
#else
    // 32-bit
    #define ptr_hash_func(ptr) kh_int_hash_func((intptr_t)ptr)
    #define ptr_equal_func(a, b) kh_int_hash_equal(a, b)
#endif
KHASH_INIT(wswl, window_t*, ws_win_info_t*, true,
           ptr_hash_func, ptr_equal_func);

typedef struct split_t {
    bool isleaf;
    bool isvertical;
    ws_win_info_t *win_info;
    float fraction;
    struct split_t *parent;
    struct split_t *frames[2];
    screen_t *screen;
} split_t;

/* workspace_t has a hashtable of workspace-specific information about each
   window.  workspace_t also has a queue of windows associated with the
   workspace but which are hidden.  This object is what gets stored in both
   the hashmap and the queue, and it represents all of the reverse pointers a
   hashmap has towards any window_t. */
struct ws_win_info_t {
    window_t *window;
    // frame which points to this window (must be NULL if window is in queue)
    split_t *frame;
    // queue element if window is hidden (must be NULL if window is in a frame)
    struct ws_win_info_t *prev;
    struct ws_win_info_t *next;
};

typedef struct {
    // one root per screen
    split_t **roots;
    size_t roots_size;
    size_t nroots;
    // the focused frame, should always be a leaf
    split_t *focus;
    // hashtable of workspace-specific information about each window
    kh_wswl_t *windows;
    // a queue of windows associated with the workspace but which are hidden
    ws_win_info_t *hidden_first;
    ws_win_info_t *hidden_last;
    // pointer to backend
    backend_t *be;
} workspace_t;

// global variables

// in-focus elements
extern split_t *g_frame;
extern workspace_t *g_workspace;

extern screen_t **g_screens;
extern size_t g_screens_size;
extern size_t g_nscreens;

extern workspace_t **g_workspaces;
extern size_t g_workspaces_size;
extern size_t g_nworkspaces;

// macros for manipulating pointer/size/count triplets

#define INIT_PTR(ptr, size, num, mincount, err) { \
    size_t newsize = 2; \
    while(newsize < (mincount) * sizeof(*ptr)){ newsize *= 2; } \
    ptr = malloc( newsize ); \
    err = (ptr == NULL); \
    if(!ptr){ \
        err = -1; \
        size = 0; \
        num = 0; \
    }else{ \
        err = 0; \
        size = newsize; \
        num = 0; \
    } \
}

#define APPEND_PTR(ptr, size, num, val, err){ \
    err = 0; \
    if(((num) + 1) * sizeof(*ptr) > size ){ \
        void *temp = realloc(ptr, size * 2); \
        if(!temp){ \
            err = -1; \
        }else{ \
            size *= 2; \
            ptr = temp; \
        } \
    } \
    if(!err){ \
        ptr[num++] = val; \
    } \
}

#define REMOVE_PTR_IDX(ptr, size, num, i){ \
    if(i == num - 1){ \
        num--; \
    }else{ \
        memmove(&ptr[i], &ptr[i+1], sizeof(*ptr) * (num - i - 1)); \
        num--; \
    } \
}

#define REMOVE_PTR(ptr, size, num, val, removed){ \
    removed = false; \
    for(size_t i = 0; i < num; i++){ \
        if(ptr[i] == val){ \
            REMOVE_PTR_IDX(ptr, size, num, i); \
            removed = true; \
            break; \
        } \
    } \
}

#define PTR_CONTAINS(ptr, size, num, val, ret){ \
    ret = false; \
    for(size_t i = 0; i < num; i++){ \
        if(ptr[i] == val){ \
            ret = true; \
            break; \
        } \
    } \
}

#define FREE_PTR(ptr, size, num){ \
    free(ptr); \
    size = 0; \
    num = 0; \
}

#endif // VENOWM_H
