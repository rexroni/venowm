CC=gcc
LD=ld
CFLAGS=-Wall -Wextra `pkg-config --cflags swc` -std=c99 -g -fdiagnostics-color=always
LDFLAGS=`pkg-config --libs swc` -lm

# BACKEND can be "SWC" (default) or "WESTON"
ifndef BACKEND
	BACKEND=SWC
endif
CFLAGS+=-DUSE_$(BACKEND)
BACKEND_OBJ=backend_$(shell echo $(BACKEND) | tr A-Z a-z).o

all: venowm test_split

test_split: split.o logmsg.o

venowm: split.o screen.o workspace.o window.o logmsg.o $(BACKEND_OBJ)

clean:
	rm -f *.o venowm test_split logmsg
