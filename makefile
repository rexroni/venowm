CC=gcc
LD=ld

PKGS=wlroots wayland-server xkbcommon
CFLAGS+=-DWLR_USE_UNSTABLE

CFLAGS+=-g
CFLAGS+=-Wall -Wextra -std=c11
CFLAGS+=-Iprotocol
CFLAGS+=`pkg-config --cflags $(PKGS)`
CFLAGS+=-fdiagnostics-color=always
CFLAGS+=-Wno-unused-parameter


LDFLAGS+=`pkg-config --libs $(PKGS)`
LDFLAGS+=-lm

XDG_DECORATION_XML=/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

all: clean venowm test_split

test_split: split.o logmsg.o

backend_wlroots.o: protocols

venowm:split.o \
       screen.o \
       workspace.o \
       window.o \
       logmsg.o \
       bindings.o \
       backend_wlroots.o

protocol/xdg-shell-protocol.c: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/xdg-shell-protocol.h: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocols: protocol/xdg-shell-protocol.h \
           protocol/xdg-shell-protocol.c

protocol/xdg-decoration-unstable-v1-private.c: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/xdg-decoration-unstable-v1-server.h: $(XDG_DECORATION_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocols: protocol/xdg-decoration-unstable-v1-server.h \
           protocol/xdg-decoration-unstable-v1-private.c


clean:
	rm -f *.o venowm test_split logmsg -r protocol
