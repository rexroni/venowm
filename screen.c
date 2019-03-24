#include <stdlib.h>
#include <stdio.h>

#include "screen.h"
#include "split.h"
#include "workspace.h"

void handle_screen_destroy(void *data){
    logmsg("destroy screen\n");
    // dereference data
    screen_t *screen = data;
    // remove the screen from the g_screens
    bool removed;
    REMOVE_PTR(g_screens, g_screens_size, g_nscreens, screen, removed);
    (void)removed;

    // we can drop the be_screen pointer, that gets handled by someone else

    // free the screen_t itself
    free(screen);

    // rerender the workspace
    workspace_rerender(g_workspace);
}

void handle_screen_geometry(void *data){
    (void)data;
    // rerender the workspace
    workspace_rerender(g_workspace);
}

int handle_screen_new(be_screen_t *be_screen, void **data){
    logmsg("new screen\n");
    screen_t *screen = malloc(sizeof(*screen));
    if(screen == NULL){
        logmsg("stderr: unable to alloc screen!\n");
        return -1;
    }
    *screen = (screen_t){0};

    // add screen to global list
    int err;
    APPEND_PTR(g_screens, g_screens_size, g_nscreens, screen, err);
    if(err){
        logmsg("stderr: unable to save screen to list!\n");
        free(screen);
        return -1;
    }

    // save the backend screen pointer
    screen->be_screen = be_screen;

    // rerender the workspace
    workspace_rerender(g_workspace);

    // return the callback data
    *data = screen;

    return 0;
}
