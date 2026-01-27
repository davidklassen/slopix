CROSS = aarch64-elf-
ROOT = $(dir $(lastword $(MAKEFILE_LIST)))..
AS = $(ROOT)/tools/as/as
AR = $(CROSS)ar

CFLAGS = -I$(CC_INCLUDE) -Iinclude

OBJ = crt0.o syscall.o stdio.o stdio_file.o string.o ctype.o test.o malloc.o stdlib.o errno.o libgen.o time.o

all: libc.a

libc.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.S
	$(CROSS)gcc -E -x assembler-with-cpp $< | $(AS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libc.a

.PHONY: all clean
