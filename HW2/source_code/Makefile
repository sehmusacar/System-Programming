CC=gcc
CFLAGS=-Wall -g

.PHONY: all clean

all: program

program: main.o
	$(CC) $(CFLAGS) main.o -o program

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o program

