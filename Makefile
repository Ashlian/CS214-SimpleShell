CC = gcc
CFLAGS = -std=c99 -g -Wall -fsanitize=address,undefined -Iinclude

build:
	mkdir -p build
	
clean:
	rm -rf build