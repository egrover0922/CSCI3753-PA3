CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread

.PHONY: all clean

all: multi_thread

multi_thread: multi_thread.o util.o
	$(CC) $(LFLAGS) $^ -o $@

multi_thread.o: multi_thread.c
	$(CC) $(CFLAGS) $<


util.o: util.c util.h
	$(CC) $(CFLAGS) $<
	
clean:
	rm -f lookup pthread-hello multi-thread
	rm -f *.o
	rm -f *~
	rm -f results.txt serviced.txt
