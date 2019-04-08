# BACKEND can be "SWC" or "WESTON" (default)
ifndef BACKEND
	BACKEND=WESTON
endif
ifeq ($(BACKEND), SWC)
	BACKEND_OBJ=backend_swc.o
	BACKEND_PKG=swc
endif
ifeq ($(BACKEND), WESTON)
	BACKEND_OBJ=backend_weston.o
	BACKEND_PKG=libweston-5 libweston-desktop-5 wayland-server
endif

CC=gcc
LD=ld
CFLAGS+=-Wall -Wextra -std=c11 -g -fdiagnostics-color=always
CFLAGS+=-DUSE_$(BACKEND)
CFLAGS+=`pkg-config --cflags $(BACKEND_PKG)`
LDFLAGS+=`pkg-config --libs $(BACKEND_PKG)`
LDFLAGS+=-lm

all: venowm test_split mini

mini: logmsg.o

test_split: split.o logmsg.o

venowm: split.o screen.o workspace.o window.o logmsg.o $(BACKEND_OBJ)

clean:
	rm -f *.o venowm test_split logmsg
