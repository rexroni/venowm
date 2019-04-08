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

DEFINE_KEY_HANDLER(quit)
    logmsg("called quit\n");
    backend_stop(be);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(exec_vimb)
    exec("env GDK_BACKEND=wayland vimb");
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(dohsplit)
    workspace_hsplit(g_workspace, g_workspace->focus, 0.5);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(dovsplit)
    workspace_vsplit(g_workspace, g_workspace->focus, 0.5);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(goleft)
    split_t *new = split_move_left(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(goright)
    split_t *new = split_move_right(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(goup)
    split_t *new = split_move_up(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(godown)
    split_t *new = split_move_down(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(remove_frame)
    workspace_remove_frame(g_workspace, g_workspace->focus);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(swapleft)
    split_t *new = split_move_left(g_workspace->focus);
    workspace_swap_windows_from_frames(g_workspace->focus, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(swapright)
    split_t *new = split_move_right(g_workspace->focus);
    workspace_swap_windows_from_frames(g_workspace->focus, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(swapup)
    split_t *new = split_move_up(g_workspace->focus);
    workspace_swap_windows_from_frames(g_workspace->focus, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(swapdown)
    split_t *new = split_move_down(g_workspace->focus);
    workspace_swap_windows_from_frames(g_workspace->focus, new);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(next_win)
    workspace_next_hidden_win_at(g_workspace, g_workspace->focus);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(prev_win)
    workspace_prev_hidden_win_at(g_workspace, g_workspace->focus);
FINISH_KEY_HANDLER

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

    be = backend_new();
    if(!be){
        goto cu_screens;
    }

#define ADD_KEY(xkey, func) \
    if(be_handle_key(be, MOD_CTRL, \
                     KEY_ ## xkey, \
                     &func, NULL)){ \
        retval = 6; \
        goto cu_backend; \
    }
#define ADD_KEY_SHIFT(xkey, func) \
    if(be_handle_key(be, MOD_CTRL | MOD_SHIFT, \
                     KEY_ ## xkey, \
                     &func, NULL)){ \
        retval = 6; \
        goto cu_backend; \
    }
    ADD_KEY(Q, quit);
    ADD_KEY(ENTER, exec_vimb);
    ADD_KEY(BACKSLASH, dohsplit);
    ADD_KEY(MINUS, dovsplit);
    ADD_KEY(H, goleft);
    ADD_KEY(J, godown);
    ADD_KEY(K, goup);
    ADD_KEY(L, goright);
    ADD_KEY(Y, remove_frame);
    ADD_KEY_SHIFT(H, swapleft);
    ADD_KEY_SHIFT(J, swapdown);
    ADD_KEY_SHIFT(K, swapup);
    ADD_KEY_SHIFT(L, swapright);
    ADD_KEY(SPACE, next_win);
    ADD_KEY_SHIFT(SPACE, prev_win);
#undef ADD_KEY

    backend_run(be);

    logmsg("post run\n");

cu_backend:
    backend_free(be);
cu_screens:
    // screens are freed by the pre-destroy-screen handler
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
