CC=gcc
CFLAGS=-Wall -Werror -Wextra -ggdb -pedantic-errors -std=c99
SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)

all: main

main: src/main.c
	$(CC) $(CFLAGS) $^ -o run -Iincludes

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $< -Iincludes

clean:
	rm -f *.o
	rm -f run
