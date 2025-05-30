CC = gcc
CFLAGS = -Wall -std=c11
LDFLAGS = -lws2_32

SRC = common.h server.c client.c

all: server.exe client.exe client_gui.exe

server.exe: server.c common.h
	$(CC) $(CFLAGS) -o $@ server.c $(LDFLAGS)

client.exe: client.c common.h
	$(CC) $(CFLAGS) -o $@ client.c $(LDFLAGS)

client_gui.exe: client_gui.c common.h
	$(CC) $(CFLAGS) -o $@ client_gui.c $(LDFLAGS)

clean:
	del /Q server.exe client.exe 2>nul || exit 0

.PHONY: all clean
