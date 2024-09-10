all: server client 

server: server.o
	gcc server.o -o server -Wextra Queue.c directory.c

server.o: server.c
	gcc -c -Wall -pedantic-errors server.c

client: client.o
	gcc client.o -o client -Wextra

client.o: client.c
	gcc -c -Wall -pedantic-errors client.c	

clean:
	rm -rf *o server client