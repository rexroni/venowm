#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "venowm_control.h"
#include "logmsg.h"
#include "venowm.h"
#include "workspace.h"
#include "split.h"

#include "protocol/venowm-shell-protocol.h"

static void venowm_control_focus_up(struct wl_client *client,
        struct wl_resource *resource){
    (void)client;

    venowm_control_t *vc = wl_resource_get_user_data(resource);

    split_t *new = split_move_up(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
    be_repaint(vc->be);
}

static void venowm_control_focus_down(struct wl_client *client,
        struct wl_resource *resource){
    (void)client;

    venowm_control_t *vc = wl_resource_get_user_data(resource);

    split_t *new = split_move_down(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
    be_repaint(vc->be);
}

static void venowm_control_focus_left(struct wl_client *client,
        struct wl_resource *resource){
    (void)client;

    venowm_control_t *vc = wl_resource_get_user_data(resource);

    split_t *new = split_move_left(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
    be_repaint(vc->be);
}

static void venowm_control_focus_right(struct wl_client *client,
        struct wl_resource *resource){
    (void)client;

    venowm_control_t *vc = wl_resource_get_user_data(resource);

    split_t *new = split_move_right(g_workspace->focus);
    workspace_focus_frame(g_workspace, new);
    be_repaint(vc->be);
}

static void venowm_control_launch(struct wl_client *client,
        struct wl_resource *resource, struct wl_array *argv_array,
        struct wl_array *argvlen_array){
    (void)client;

    venowm_control_t *vc = wl_resource_get_user_data(resource);
    (void)vc;

    uint32_t *argvlen_ptr;

    // validity checks on the arg list
    size_t argv_sum_len = argv_array->size;
    size_t nargs = argvlen_array->size / sizeof(*argvlen_ptr);

    // make sure argv is not an empty list
    if(argv_sum_len == 0 || nargs == 0){
        logmsg("launch arguments are empty, not executing\n");
        goto done;
    }

    // make sure the argv_sum_length is correct
    size_t counted_sum_len = 0;
    wl_array_for_each(argvlen_ptr, argvlen_array){
        counted_sum_len += *argvlen_ptr;
    }

    if(counted_sum_len != argv_sum_len){
        logmsg("launch arguments improperly packed, not executing\n");
        goto done;
    }

    // make sure all args are null-terminated
    char *p = argv_array->data;
    wl_array_for_each(argvlen_ptr, argvlen_array){
        if(p[*argvlen_ptr - 1] != '\0'){
            logmsg("launch arguments not null terminated, not executing\n");
            goto done;
        }
        p += *argvlen_ptr;
    }

    // build the char **argv for execvp, which is NULL terminated.
    size_t argv_copy_size = sizeof(char*) * (nargs + 1);
    char **argv_copy = malloc(argv_copy_size);
    if(!argv_copy){
        logmsg("failed to launch %s: %m\n", (char*)argv_array->data);
        goto done;
    }

    // fill argv_copy_size with pointers to argv_array->data
    size_t argv_idx = 0;
    p = argv_array->data;
    wl_array_for_each(argvlen_ptr, argvlen_array){
        argv_copy[argv_idx++] = p;
        p += *argvlen_ptr;
    }
    // null-terminate the list
    argv_copy[argv_idx] = NULL;

    // actually launch the child process
    pid_t pid = fork();
    if(pid < 0){
        logmsg("failed to launch %s: %m\n", (char*)argv_array->data);
        goto cu_argv_copy;
    }
    if(pid == 0){
        // child process

        // all input/output uses /dev/null (best effort)
        int null_fd = open("/dev/null", O_RDWR);
        if(null_fd < 0){
            perror("opening /dev/null");
        }else{
            // ignore errors, it'll be fine
            close(0);
            dup(null_fd);
            close(1);
            dup(null_fd);
            close(2);
            dup(null_fd);
        }

        // actually launch the command
        execvp(argv_copy[0], argv_copy);
        perror("execvp");
        exit(127);
    }

cu_argv_copy:
    free(argv_copy);
done:
    return;
}

static const struct venowm_control_interface venowm_control_impl = {
    venowm_control_focus_up,
    venowm_control_focus_down,
    venowm_control_focus_left,
    venowm_control_focus_right,
    venowm_control_launch,
};

static void unbind_venowm_control(struct wl_resource *resource){
    venowm_control_t *vc = wl_resource_get_user_data(resource);
    (void)vc;

    logmsg("venowm_control client disconnected\n");
    // it seems you should not call wl_resource_destroy here...
}

static void bind_venowm_control(struct wl_client *client, void *data,
        uint32_t version, uint32_t id){
    venowm_control_t *vc = data;
    struct wl_resource *resource;

    logmsg("new venowm_control client\n");

    resource = wl_resource_create(client, &venowm_control_interface, 1, id);
    if(!resource){
        wl_client_post_no_memory(client);
        return;
    }

    wl_resource_set_implementation(resource, &venowm_control_impl, vc,
            unbind_venowm_control);
}


void venowm_control_free(venowm_control_t *vc){
    wl_global_destroy(vc->global);
    free(vc);
}

venowm_control_t *venowm_control_new(backend_t *be,
        struct wl_display *display){
    venowm_control_t *vc = malloc(sizeof(*vc));
    if(!vc) return NULL;
    *vc = (venowm_control_t){
        .be=be,
    };

    vc->global = wl_global_create(display, &venowm_control_interface, 1,
            vc, bind_venowm_control);
    if(!vc->global) goto fail_malloc;

    return vc;

fail_malloc:
    free(vc);
    return NULL;
}
