CC = gcc
CFLAGS = -g

all: Server Client

default: Server Client

Server: Server.c
	$(CC) $(CFLAGS) -pthread -o Server Server.c

Client: Client.c
	$(CC) $(CFLAGS) -o Client Client.c

clean:
	rm -f Server Client Server_thread *.o