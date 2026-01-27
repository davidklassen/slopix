CROSS = aarch64-elf-
ROOT = $(dir $(lastword $(MAKEFILE_LIST)))../..
AS_SRC = $(ROOT)/tools/as

AS = $(CROSS)as
LD = $(CROSS)ld

CFLAGS = -I$(CC_INCLUDE) -I$(LIBC_INCLUDE) -I$(AS_SRC)
LDFLAGS = -nostdlib -T ../link.ld

SRCS = main.c lexer.c parser.c encode.c symtab.c section.c strtab.c reloc.c elf_write.c literal.c
OBJS = $(SRCS:.c=.o)

as.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBC)

%.o: $(AS_SRC)/%.c $(AS_SRC)/as.h
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s as.elf

.PHONY: clean
