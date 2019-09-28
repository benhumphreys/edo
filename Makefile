CC=gcc
CFLAGS=-g -Wall -std=c99

all: edo

edo: edo.c
	$(CC) edo.c -o edo $(CFLAGS)

clean:
	rm -f edo *.o
