#ifndef WORKSPACE_H
#define WORKSPACE_H

#include "venowm.h"

workspace_t *workspace_new(void);

// frees all of its roots, downrefs all of its windows
void workspace_free(workspace_t *workspace);

// uprefs window and adds it to the workspace
void workspace_add_window(workspace_t *ws, window_t *window);

// unmap all windows in workspace
void workspace_hide(workspace_t *ws);
// restore windows to their frames, and render to the existing screens
void workspace_restore(workspace_t *ws);

// rerender to existing screens, such as after screen add/delete
// (as long as window mapping/unmapping is idempotent, this is sufficient:)
#define workspace_rerender workspace_restore

#endif // WORKSPACE_H
