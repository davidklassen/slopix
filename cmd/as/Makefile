# Native assembler: compiled by chibicc, runs on slopix
# CC, AS, LD, LIBC, LIBC_INCLUDE must be passed from parent

CFLAGS = -I$(LIBC_INCLUDE)

SRCS = main.c lexer.c parser.c encode.c symtab.c section.c strtab.c reloc.c elf_write.c literal.c
OBJS = $(SRCS:.c=.o)

as.elf: $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBC)

%.o: %.c as.h
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s as.elf

.PHONY: clean
