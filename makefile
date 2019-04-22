CC=gcc
LD=ld

PKGS=wlroots wayland-server
CFLAGS+=-DWLR_USE_UNSTABLE

CFLAGS+=-g
CFLAGS+=-Wall -Wextra -std=c11
CFLAGS+=`pkg-config --cflags $(PKGS)`
CFLAGS+=-fdiagnostics-color=always
CFLAGS+=-Wno-unused-parameter


LDFLAGS+=`pkg-config --libs $(PKGS)`
LDFLAGS+=-lm

all: clean venowm test_split

test_split: split.o logmsg.o

venowm:split.o screen.o workspace.o window.o logmsg.o bindings.o backend_wlroots.o

protocol/xdg-decoration-unstable-v1-private.c: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/xdg-decoration-unstable-v1-server.h: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocols: protocol/xdg-decoration-unstable-v1-server.h \
           protocol/xdg-decoration-unstable-v1-private.c


clean:
	rm -f *.o venowm test_split logmsg
