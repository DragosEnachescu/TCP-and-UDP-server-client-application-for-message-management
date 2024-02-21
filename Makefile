CC=g++
CFLAGS=-c -g -Wall -Wextra -std=c++17

all: server subscriber

server: server.o
		$(CC) server.o -o server

server.o: server.cpp
		$(CC) $(CFLAGS) $^

subscriber: subscriber.o
		$(CC) subscriber.o -o subscriber

subscriber.o: subscriber.cpp
		$(CC) $(CFLAGS) $^

clean:
		rm -f *.o server subscriber