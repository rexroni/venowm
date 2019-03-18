CC=gcc
LD=ld
CFLAGS=-Wall -Wextra `pkg-config --cflags swc` -std=c99 -g -fdiagnostics-color=always
LDFLAGS=`pkg-config --libs swc` -lm

all: venowm test_split

test_split: split.o window.o logmsg.o workspace.o

venowm: split.o screen.o workspace.o window.o logmsg.o

clean:
	rm -f *.o venowm test_split logmsg
