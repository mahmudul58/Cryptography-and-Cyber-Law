CC     = cc
CFLAGS = -Wall -Wextra -O2

all: server client

server: server.c layers.c layers.h netutil.h
	$(CC) $(CFLAGS) -o server server.c layers.c

client: client.c layers.c layers.h netutil.h
	$(CC) $(CFLAGS) -o client client.c layers.c

clean:
	rm -f server client *.o

.PHONY: all clean
