CC = gcc
CFLAGS = -Wall -std=c11
LDFLAGS = -lws2_32

SRC = common.h server.c client.c

all: server.exe client.exe

server.exe: server.c common.h
	$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

client.exe: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

clean:
	del /Q server.exe client.exe 2>nul || exit 0

.PHONY: all clean
