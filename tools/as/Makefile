# Cross-assembler: builds with host compiler, runs on development machine
# Source lives in cmd/as/, this Makefile just builds the cross version

CC ?= cc
CFLAGS = -std=c11 -g -Wall -Wextra -O0

SRC_DIR = ../../cmd/as
SRCS = main.c lexer.c parser.c encode.c symtab.c section.c strtab.c reloc.c elf_write.c literal.c
OBJS = $(SRCS:.c=.o)

as: $(OBJS)
	$(CC) -o $@ $^

%.o: $(SRC_DIR)/%.c $(SRC_DIR)/as.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f as $(OBJS)

test: as
	./tests/run-tests.sh

test-unit: as
	./tests/run-tests.sh --unit-only

.PHONY: clean test test-unit
