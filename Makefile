CC = gcc
C++ = g++

CFLAGS   = -g -Wall
C++FLAGS = -g -Wall

http-server : http-server.c

.PHONY: clean
clean:
	rm -f *.o a.out core http-server

.PHONY: all
all: clean http-server
