ROOT = $(CURDIR)/../../..

CC = ../chibicc
AS = aarch64-elf-as
LD = aarch64-elf-ld

CFLAGS = -I. -I$(ROOT)/libc/include -I../include
LIBC = $(ROOT)/libc/libc.a
LINK_SCRIPT = $(ROOT)/cmd/link.ld

# Test file - only one at a time (each has its own main)
TESTS = pointer

OBJS = common.o $(addsuffix .o,$(TESTS))

all: tests.elf

tests.elf: $(OBJS) $(LIBC)
	$(LD) -T $(LINK_SCRIPT) -o $@ $(OBJS) $(LIBC)

%.o: %.c ../chibicc
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s tests.elf

.PHONY: all clean
