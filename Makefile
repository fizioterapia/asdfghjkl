CC := gcc
CFLAGS := -Wall -Werror -lraylib -lm

build-and-run: main
	./main

main: main.c
main.c:

format:
	clang-format --style=webkit -i main.c

clean:
	rm -f main
