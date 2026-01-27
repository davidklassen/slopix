# Native assembler: compiled by chibicc, assembled by our cross-assembler
# This is proper bootstrapping - the assembler assembles itself

ROOT = $(dir $(lastword $(MAKEFILE_LIST)))../..

AS ?= $(ROOT)/tools/as/as
LD ?= aarch64-elf-ld

CFLAGS = -I$(CC_INCLUDE) -I$(LIBC_INCLUDE)
LDFLAGS = -nostdlib -T ../link.ld

SRCS = main.c lexer.c parser.c encode.c symtab.c section.c strtab.c reloc.c elf_write.c literal.c
OBJS = $(SRCS:.c=.o)

as.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBC)

%.o: %.c as.h
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s as.elf

.PHONY: clean
