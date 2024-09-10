CC = gcc
CFLAGS = -pthread -Wall
TARGETS = PideShop HungryVeryMuch

all: $(TARGETS)

PideShop: server.c
	$(CC) $(CFLAGS) -o PideShop server.c

HungryVeryMuch: client.c
	$(CC) $(CFLAGS) -o HungryVeryMuch client.c

clean:
	rm -f $(TARGETS) *.o
