CC=gcc
LD=ld

PKGS=wlroots wayland-client wayland-server xkbcommon
CFLAGS+=-DWLR_USE_UNSTABLE

CFLAGS+=-g
CFLAGS+=-Wall -Wextra -std=c11
CFLAGS+=-Iprotocol
CFLAGS+=`pkg-config --cflags $(PKGS)`
CFLAGS+=-fdiagnostics-color=always
CFLAGS+=-Wno-unused-parameter


LDFLAGS+=`pkg-config --libs $(PKGS)`
LDFLAGS+=-lm

XDG_SHELL_XML=/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

all: venowm test_split

test_split: split.o logmsg.o

backend_wlroots.o: protocol/xdg-shell-protocol.h \
                   protocol/xdg-shell-protocol.c

venowm_control.o: protocol/venowm-shell-protocol.h \
                  protocol/venowm-shell-protocol.c

libvenowm.o: protocol/venowm-shell-client-protocol.h \
             protocol/venowm-shell-protocol.c

venowm:split.o \
       screen.o \
       workspace.o \
       window.o \
       logmsg.o \
       bindings.o \
       venowm_control.o \
       backend_wlroots.o \
       libvenowm.o

protocol/xdg-shell-protocol.h: $(XDG_SHELL_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocol/xdg-shell-client-protocol.h: $(XDG_SHELL_XML)
	wayland-scanner client-header $< $@

protocol/xdg-shell-protocol.c: $(XDG_SHELL_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/venowm-shell-protocol.h: venowm-shell.xml
	mkdir -p protocol
	wayland-scanner server-header $< $@

protocol/venowm-shell-client-protocol.h: venowm-shell.xml
	wayland-scanner client-header $< $@

protocol/venowm-shell-protocol.c: venowm-shell.xml
	mkdir -p protocol
	wayland-scanner private-code $< $@

.PHONY: protocols
protocols: protocol/xdg-shell-protocol.h \
           protocol/xdg-shell-client-protocol.h \
           protocol/xdg-shell-protocol.c \
           protocol/venowm-shell-protocol.h \
           protocol/venowm-shell-client-protocol.h \
           protocol/venowm-shell-protocol.c \

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
