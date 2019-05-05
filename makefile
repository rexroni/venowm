CC=gcc
LD=ld

# Hm... the libweston package is versioned explicitly but weston is not.  This
# way of doing things is not safe.
PKGS=weston libweston-6 libweston-desktop-6 wayland-server cairo freetype2

CFLAGS+=-g
CFLAGS+=-Wall -Wextra -std=c11
CFLAGS+=-fPIC -Iprotocol
CFLAGS+=`pkg-config --cflags $(PKGS)`
CFLAGS+=-fdiagnostics-color=always

LDFLAGS+=`pkg-config --libs $(PKGS)`
LDFLAGS+=-lm
LDFLAGS+=-shared

XDG_DECORATION_XML=/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml

all: clean venowm_shell_plugin.so test_split

test_split: split.o logmsg.o

venowm_shell_plugin.so: protocols \
                        venowm_shell_plugin.o \
                        split.o \
                        screen.o \
                        workspace.o \
                        window.o \
                        logmsg.o \
                        bindings.o \
	$(CC) $(LDFLAGS) -o $@ $^

protocol/xdg-decoration-unstable-v1-private.c: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/xdg-decoration-unstable-v1-server.h: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocols: protocol/xdg-decoration-unstable-v1-server.h \
           protocol/xdg-decoration-unstable-v1-private.c


clean:
	rm -f *.o venowm_shell_plugin.so test_split logmsg -r protocol
