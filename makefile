CC=gcc
LD=ld

# Hm... the libweston package is versioned explicitly but weston is not.  This
# way of doing things is not safe.
PKGS=weston libweston-6 libweston-desktop-6 wayland-server

CFLAGS+=-g
CFLAGS+=-Wall -Wextra -std=c11
CFLAGS+=-fPIC
CFLAGS+=`pkg-config --cflags $(PKGS)`
CFLAGS+=-fdiagnostics-color=always

LDFLAGS+=`pkg-config --libs $(PKGS)`
LDFLAGS+=-lm
LDFLAGS+=-shared

all: clean venowm_shell_plugin.so test_split

test_split: split.o logmsg.o

venowm_shell_plugin.so: venowm_shell_plugin.o split.o screen.o workspace.o window.o logmsg.o bindings.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o venowm_shell_plugin.so test_split logmsg
