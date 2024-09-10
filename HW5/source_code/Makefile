# Variables
CC = gcc
CFLAGS = -Wall -pthread
TARGET = MWCp
SOURCES = 161044085_main.c

# Targets
all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
