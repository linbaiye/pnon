
all: client server

CFLAGS += -std=c99 -g -O0 -D_GNU_SOURCE

client: log.o client_main.o protocol.o client.o
	gcc -o $@ $^

server: log.o server_main.o protocol.o server.o
	gcc -o $@ $^

clean:
	bash -c 'rm -f *.o >/dev/null 2>&1'
	bash -c 'rm -f ./client >/dev/null 2>&1'
	bash -c 'rm -f ./server >/dev/null 2>&1'
