#include <unistd.h>
#include <linux/input.h>
#include <stdio.h>
#include <xkbcommon/xkbcommon.h>

#include "bindings.h"
#include "logmsg.h"
#include "venowm.h"
#include "workspace.h"
#include "split.h"
#include "backend.h"

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

#define DEFINE_KEY_HANDLER(func_name) \
    bool func_name(backend_t *be, void *data){ \
        bool retval = true; \
        (void)data;

#define FINISH_KEY_HANDLER \
        be_repaint(be); \
        return retval; \
    }

DEFINE_KEY_HANDLER(quit)
    logmsg("called quit\n");
    backend_stop(be);
FINISH_KEY_HANDLER

DEFINE_KEY_HANDLER(exec_weston_terminal)
    exec("weston-terminal");
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

DEFINE_KEY_HANDLER(close_window)
    if(g_workspace->focus->win_info){
        be_window_close(g_workspace->focus->win_info->window->be_window);
    }
FINISH_KEY_HANDLER


#define ADD_KEY(xkey, func) \
    if(be_handle_key(be, MOD_CTRL, \
                     XKB_KEY_ ## xkey, \
                     &func, be)){ \
        goto fail; \
    }
#define ADD_KEY_SHIFT(xkey, func) \
    if(be_handle_key(be, MOD_CTRL | MOD_SHIFT, \
                     XKB_KEY_ ## xkey, \
                     &func, be)){ \
        goto fail; \
    }

int add_bindings(backend_t *be){
    ADD_KEY(q, quit);
    ADD_KEY(Return, exec_weston_terminal);
    ADD_KEY(backslash, dohsplit);
    ADD_KEY(minus, dovsplit);
    ADD_KEY(h, goleft);
    ADD_KEY(j, godown);
    ADD_KEY(k, goup);
    ADD_KEY(l, goright);
    ADD_KEY(y, remove_frame);
    ADD_KEY(i, close_window);
    ADD_KEY_SHIFT(h, swapleft);
    ADD_KEY_SHIFT(j, swapdown);
    ADD_KEY_SHIFT(k, swapup);
    ADD_KEY_SHIFT(l, swapright);
    ADD_KEY(space, next_win);
    ADD_KEY_SHIFT(space, prev_win);
    return 0;

fail:
    return -1;
}
