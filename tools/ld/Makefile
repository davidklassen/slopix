# Cross-linker: builds with host compiler, runs on development machine
# Source lives in cmd/ld/, this Makefile just builds the cross version

CC ?= cc
CFLAGS = -std=c11 -g -Wall -Wextra -O0

SRC_DIR = ../../cmd/ld
SRCS = main.c elf_read.c symbol.c section.c reloc.c output.c archive.c
OBJS = $(SRCS:.c=.o)

ld: $(OBJS)
	$(CC) -o $@ $^

%.o: $(SRC_DIR)/%.c $(SRC_DIR)/ld.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f ld $(OBJS)

test: ld
	./tests/run-tests.sh

.PHONY: clean test
