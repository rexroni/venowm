#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <signal.h>
#include <wait.h>

// #include <xkbcommon/xkbcommon.h>
#include <linux/input.h>

#include "venowm.h"
#include "split.h"
#include "screen.h"
#include "window.h"
#include "workspace.h"
#include "bindings.h"

// backend_t, needed for keybindings
static backend_t *be;

// venowm global variables
workspace_t *g_workspace;

screen_t **g_screens;
size_t g_screens_size;
size_t g_nscreens;

workspace_t **g_workspaces;
size_t g_workspaces_size;
size_t g_nworkspaces;

void sigchld_handler(int signum){
    logmsg("handled sigchld\n");
    (void)signum;
    int wstatus;
    wait(&wstatus);
    if(WIFEXITED(wstatus)){
        // exited normally, now we can safely check exit code
        int exit_code = WEXITSTATUS(wstatus);
        if(exit_code == 0){
            // nothing went wrong!
            logmsg("handled sigchld 0\n");
            return;
        }
    }
    // TODO: tell the user if something went wrong
}

int main(){
    int retval = 0;
    int err;


    // set the SIGCHLD handler
    signal(SIGCHLD, sigchld_handler);

    be = backend_new();
    if(!be){;
        return 99;
    }

    INIT_PTR(g_workspaces, g_workspaces_size, g_nworkspaces, 8, err);
    if(err){
        retval = 99;
        goto cu_backend;
    }

    // allocate some workspaces
    for(size_t i = 0; i <= 5; i ++){
        workspace_t *new = workspace_new(be);
        if(!new){
            retval = 99;
            goto cu_workspaces;
        }
        int err;
        APPEND_PTR(g_workspaces, g_workspaces_size, g_nworkspaces, new, err);
        if(err){
            workspace_free(new);
            retval = 99;
            goto cu_workspaces;
        }
    }

    // set first workspace
    g_workspace = g_workspaces[0];

    INIT_PTR(g_screens, g_screens_size, g_nscreens, 4, err);
    if(err){
        retval = 99;
        goto cu_workspaces;
    }

    if(add_bindings(be)) goto cu_workspaces;

    backend_run(be);

    logmsg("post run\n");

//cu_screens:
    // screens are freed by the pre-destroy-screen handler
    FREE_PTR(g_screens, g_screens_size, g_nscreens);
cu_workspaces:
    // but we have to manually free workspaces
    for(size_t i = 0; i < g_nworkspaces; i++){
        workspace_free(g_workspaces[i]);
    }
    FREE_PTR(g_workspaces, g_workspaces_size, g_nworkspaces);
cu_backend:
    backend_free(be);
    logmsg("exiting from main: %d\n", retval);
    return retval;
}
