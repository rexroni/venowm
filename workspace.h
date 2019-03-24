#ifndef WORKSPACE_H
#define WORKSPACE_H

#include <limits.h>

#include "venowm.h"

workspace_t *workspace_new(void);

// frees all of its roots, downrefs all of its windows
void workspace_free(workspace_t *workspace);

// uprefs window and adds it to the workspace
void workspace_add_window(workspace_t *ws, window_t *window, bool map_now);
// removes a window from the workspace and downrefs it
void workspace_remove_window(workspace_t *ws, window_t *window);
// removes a window from a frame and put it in the hidden list
void workspace_remove_window_from_frame(workspace_t *ws, split_t *split,
                                        bool prepend_old_window);

// unmap all windows in workspace
void workspace_hide(workspace_t *ws);
// restore windows to their frames, and render to the existing screens
void workspace_restore(workspace_t *ws);

// rerender to existing screens, such as after screen add/delete
// (as long as window mapping/unmapping is idempotent, this is sufficient:)
#define workspace_rerender workspace_restore

// trigger workspace to update window focus
void workspace_focus_frame(workspace_t *ws, split_t *split);

void workspace_vsplit(workspace_t *ws, split_t *split, float fraction);
void workspace_hsplit(workspace_t *ws, split_t *split, float fraction);

void workspace_remove_frame(workspace_t *ws, split_t *frame);

void workspace_swap_windows_from_frames(split_t *src, split_t *dst);

void workspace_next_hidden_win_at(workspace_t *ws, split_t *split);
void workspace_prev_hidden_win_at(workspace_t *ws, split_t *split);

#endif // WORKSPACE_H
