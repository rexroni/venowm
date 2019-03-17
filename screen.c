#include <stdlib.h>
#include <stdio.h>

#include "screen.h"
#include "split.h"
#include "workspace.h"

static screen_t *screen_new(void){
    screen_t *out = malloc(sizeof(*out));
    if(!out) return NULL;
    *out = (screen_t){0};
    return out;
}

void handle_screen_destroy(void *data){
    logmsg("destroy screen\n");
    // dereference data
    screen_t *screen = data;
    // remove the screen from the g_screens
    bool removed;
    REMOVE_PTR(g_screens, g_screens_size, g_nscreens, screen, removed);
    (void)removed;

    // libswc owns the swc_screen pointer inside screen_t, we don't free it.

    // free the screen_t itself
    free(screen);

    // rerender the workspace
    workspace_rerender(g_workspace);
}

void handle_screen_geometry_change(void *data){
    (void)data;
    // rerender the workspace
    workspace_rerender(g_workspace);
}

static struct swc_screen_handler screen_handler = {
    .destroy = handle_screen_destroy,
    .geometry_changed = handle_screen_geometry_change,
    .usable_geometry_changed = handle_screen_geometry_change,
    .entered = NULL,
};

void handle_new_screen(struct swc_screen *swc_screen){
    logmsg("new screen\n");
    screen_t *screen = screen_new();
    if(screen == NULL){
        logmsg("stderr: unable to alloc screen!\n");
        exit(99);
    }
    // save the swc pointer
    screen->swc_screen = swc_screen;
    // add screen to global list
    int err;
    APPEND_PTR(g_screens, g_screens_size, g_nscreens, screen, err);
    if(err){
        logmsg("stderr: unable to save screen to list!\n");
        exit(99);
    }

    // rerender the workspace
    workspace_rerender(g_workspace);

    swc_screen_set_handler(swc_screen, &screen_handler, screen);
}
