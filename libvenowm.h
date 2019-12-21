#ifndef LIBVENOWM_H
#define LIBVENOWM_H

#include <wayland-client.h>

// the venowm client object
struct venowm;

struct venowm *venowm_create();
void venowm_destroy(struct venowm *v);

// return the last error message (buffer is part of struct venowm)
const char *venowm_errmsg(struct venowm *v);

// all remaining functions return -1 on error or 0 on success:

/* Call venowm_connect before any other commands.  Set display to non-null to
   reuse an existing wayland display connection. */
int venowm_connect(struct venowm *v, struct wl_display *wl_display);

// flush any pending commands (without having to issue an additional command)
int venowm_flush(struct venowm *v);

/* Send commands to venowm.  Setting flush=false will tell venowm to wait to
   render the command until either a command is sent with flush=true or
   venowm_flush is called. */
int venowm_focus_up(struct venowm *v, bool flush);
int venowm_focus_down(struct venowm *v, bool flush);
int venowm_focus_left(struct venowm *v, bool flush);
int venowm_focus_right(struct venowm *v, bool flush);

#endif // LIBVENOWM_H
