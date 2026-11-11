CC = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined -Iinclude

all: build/mysh

build:
	mkdir -p build

build/main.o: src/main.c include/mysh.h | build
	$(CC) $(CFLAGS) -c src/main.c -o build/main.o

build/reader.o: src/reader.c include/mysh.h | build
	$(CC) $(CFLAGS) -c src/reader.c -o build/reader.o

build/parser.o: src/parser.c include/mysh.h | build
	$(CC) $(CFLAGS) -c src/parser.c -o build/parser.o

build/execute.o: src/execute.c include/mysh.h | build
	$(CC) $(CFLAGS) -c src/execute.c -o build/execute.o

build/mysh: build/main.o build/reader.o build/parser.o build/execute.o
	$(CC) $(CFLAGS) build/main.o build/reader.o build/parser.o build/execute.o -o build/mysh
	
clean:
	rm -rf build

.PHONY: all clean