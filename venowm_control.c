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

static const struct venowm_control_interface venowm_control_impl = {
    venowm_control_focus_up,
    venowm_control_focus_down,
    venowm_control_focus_left,
    venowm_control_focus_right,
};

static void unbind_venowm_control(struct wl_resource *resource){
    venowm_control_t *vc = wl_resource_get_user_data(resource);
    (void)vc;

    // TODO: figure out what to do
    logmsg("unbinding venowm control, not sure what to do");
}

static void bind_venowm_control(struct wl_client *client, void *data,
        uint32_t version, uint32_t id){
    venowm_control_t *vc = data;
    struct wl_resource *resource;

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

