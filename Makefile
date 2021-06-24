CC=gcc
CFLAGS=-Wall -std=c99
LDFLAGS= -static
SOURCES= main.c iso.c
EXECUTABLE=regps3iso
all:
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $(EXECUTABLE)
clean:
	rm -rf $(EXECUTABLE)
