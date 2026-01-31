# Native linker: compiled by chibicc, runs on slopix
# CC, AS, LD, LIBC, LIBC_INCLUDE must be passed from parent

CFLAGS = -I$(LIBC_INCLUDE)

SRCS = main.c elf_read.c symbol.c section.c reloc.c output.c archive.c
OBJS = $(SRCS:.c=.o)

ld.elf: $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBC)

%.o: %.c ld.h
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o $(SRCS:.c=.s) ld.elf

.PHONY: clean
