#ifndef VENOWM_H
#define VENOWM_H

#include <string.h>
#include <stdbool.h>

#include "logmsg.h"

// the main data types.  Some are interdependent.

typedef struct {
    struct swc_screen *swc_screen;
    // more data to come at a later time
} screen_t;

typedef struct {
    int refs; // how many workspaces is this window in?
    struct swc_window *swc_window;
    // windows may close or die while there are still open refs to this struct
    bool isvalid;
} window_t;

typedef struct split_t {
    bool isleaf;
    bool isvertical;
    window_t *window; // NULL means there's no window in this frame
    float fraction;
    struct split_t *parent;
    struct split_t *frames[2];
    screen_t *screen; // not used internally to split.c
} split_t;

typedef struct {
    window_t **windows;
    size_t windows_size;
    size_t nwindows;
    // one root per split
    split_t **roots;
    size_t roots_size;
    size_t nroots;
    // the focused frame, should always be a leaf
    split_t *focus;
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
    for(size_t i = 0; i < num; i ++){ \
        if(ptr[i] == val){ \
            REMOVE_PTR_IDX(ptr, size, num, i); \
            removed = true; \
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
