CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
LDFLAGS_CLIENT = -pthread

all: server client

server: server.c common.h
	$(CC) $(CFLAGS) -o $@ server.c

client: client.c common.h
	$(CC) $(CFLAGS) $(LDFLAGS_CLIENT) -o $@ client.c

clean:
	rm -f server client

.PHONY: all clean
