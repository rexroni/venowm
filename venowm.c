// for setenv:
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <string.h>
#include <swc.h>
#include <unistd.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <xkbcommon/xkbcommon.h>
#include <signal.h>
#include <wait.h>

#include "venowm.h"
#include "split.h"
#include "screen.h"
#include "window.h"
#include "workspace.h"

// libswc global variables
static struct wl_display *disp;
static struct wl_event_loop *event_loop;

// venowm global variables
workspace_t *g_workspace;

screen_t **g_screens;
size_t g_screens_size;
size_t g_nscreens;

workspace_t **g_workspaces;
size_t g_workspaces_size;
size_t g_nworkspaces;

static const struct swc_manager manager = {.new_screen=&handle_new_screen,
                                           .new_window=&handle_new_window};

static void quit(void *data, uint32_t time, uint32_t value, uint32_t state){
    logmsg("called quit\n");
    (void)data;
    (void)time;
    (void)value;
    if(state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    wl_display_terminate(disp);
}

static void exec(const char *shcmd){
    logmsg("called exec\n");
    pid_t pid = fork();
    if(pid < 0){
        perror("fork");
        return;
    }
    if(pid == 0){
        // child
        execl("/bin/sh", "/bin/sh", "-c", shcmd, NULL);
        perror("execl");
        exit(127);
    }
    // parent continues with whatever it was doing
    return;
}

static void exec_vimb(void *data, uint32_t time, uint32_t value, uint32_t state){
    (void)data;
    (void)time;
    (void)value;
    if(state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    exec("env GDK_BACKEND=wayland vimb");
}

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

    INIT_PTR(g_workspaces, g_workspaces_size, g_nworkspaces, 8, err);
    if(err){
        return 99;
    }

    // allocate some workspaces
    for(size_t i = 0; i <= 5; i ++){
        workspace_t *new = workspace_new();
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

    disp = wl_display_create();
    if(disp == NULL){
        retval = 2;
        goto cu_screens;
    }

    const char *wl_sock = wl_display_add_socket_auto(disp);
    if(wl_sock == NULL){
        retval = 3;
        goto cu_display;
    }

    if(setenv("WAYLAND_DISPLAY", wl_sock, 0)){
        perror("set environment");
        retval = 4;
        goto cu_display;
    }

    if(!swc_initialize(disp, NULL, &manager)){
        retval = 5;
        goto cu_display;
    }

    if(swc_add_binding(SWC_BINDING_KEY,
                       SWC_MOD_CTRL,
                       XKB_KEY_q,
                       &quit, NULL)){
        retval = 6;
        goto cu_swc;
    }

    if(swc_add_binding(SWC_BINDING_KEY,
                       SWC_MOD_CTRL,
                       XKB_KEY_Return,
                       &exec_vimb, NULL)){
        retval = 6;
        goto cu_swc;
    }

    event_loop = wl_display_get_event_loop(disp);
    if(event_loop == NULL){
        retval = 7;
        goto cu_swc;
    }

    wl_display_run(disp);

cu_swc:
    swc_finalize();
cu_display:
    wl_display_destroy(disp);
cu_screens:
    // screens *should* be freed by the pre-destroy-screen handler
    FREE_PTR(g_screens, g_screens_size, g_nscreens);
cu_workspaces:
    // but we have to manually free workspaces
    for(size_t i = 0; i < g_nworkspaces; i++){
        workspace_free(g_workspaces[i]);
    }
    FREE_PTR(g_workspaces, g_workspaces_size, g_nworkspaces);
    logmsg("exiting from main: %d\n", retval);
    return retval;
}
