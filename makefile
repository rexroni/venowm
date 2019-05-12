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

PROTOCOLS=protocol/xdg-shell-protocol.h \
          protocol/xdg-shell-protocol.c

XDG_SHELL_XML=/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml

all: venowm test_split

test_split: split.o logmsg.o

backend_wlroots.o: $(PROTOCOLS)

venowm:split.o \
       screen.o \
       workspace.o \
       window.o \
       logmsg.o \
       bindings.o \
       backend_wlroots.o

protocol/xdg-shell-protocol.c: $(XDG_SHELL_XML)
	mkdir -p protocol
	wayland-scanner private-code $< $@

protocol/xdg-shell-protocol.h: $(XDG_SHELL_XML)
	mkdir -p protocol
	wayland-scanner server-header $< $@

.PHONY: protocols
protocols: protocol/xdg-shell-protocol.h \
           protocol/xdg-shell-protocol.c

clean:
	rm -f *.o venowm test_split logmsg -r protocol
