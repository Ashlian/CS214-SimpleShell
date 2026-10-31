CC = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined -Iinclude

all: build/mysh

build:
	mkdir -p build

build/mysh.o: src/mysh.c include/mysh.h | build
	$(CC) $(CFLAGS) -c src/mysh.c -o build/mysh.o

build/mysh: build/mysh.o
	$(CC) $(CFLAGS) build/mysh.o -o build/mysh
	
clean:
	rm -rf build

.PHONY: all clean