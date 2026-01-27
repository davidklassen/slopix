CC ?= cc
CFLAGS = -std=c11 -g -Wall -Wextra -O0

SRCS = main.c lexer.c parser.c encode.c symtab.c section.c strtab.c reloc.c elf_write.c literal.c
OBJS = $(SRCS:.c=.o)

as: $(OBJS)
	$(CC) -o $@ $^

%.o: %.c as.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f as $(OBJS)

test: as
	./tests/run-tests.sh

test-unit: as
	./tests/run-tests.sh --unit-only

.PHONY: clean test test-unit
