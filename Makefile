CC=gcc
CFLAGS=-Wall -Werror -Wextra -ggdb -pedantic-errors -std=c11
SRC=$(shell find src/ ! -name "main.c" -name "*.c")
OBJ=$(SRC:.c=.o)

all: main
.PHONY: tests

main: src/main.c $(SRC)
	$(CC) $(CFLAGS) $^ -o run

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

tests: tests/main.c $(SRC)
	@$(CC) $(CFLAGS) $^ -o run_tests
	@-./run_tests
	@rm -f run_tests

clean:
	rm -f *.o
	rm -f run
