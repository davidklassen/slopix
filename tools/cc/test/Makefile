ROOT = $(CURDIR)/../../..

CC = ../chibicc
AS = aarch64-elf-as
LD = aarch64-elf-ld

CFLAGS = -I. -I$(ROOT)/libc/include -I../include
LIBC = $(ROOT)/libc/libc.a
LINK_SCRIPT = $(ROOT)/cmd/link.ld

TESTS = float struct union bitfield

ELFS = $(addsuffix .elf,$(TESTS))

all: $(ELFS)

%.elf: %.o common.o $(LIBC)
	$(LD) -T $(LINK_SCRIPT) -o $@ $< common.o $(LIBC)

%.o: %.c ../chibicc
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s *.elf

.PHONY: all clean
