CROSS = aarch64-elf-
ROOT = $(dir $(lastword $(MAKEFILE_LIST)))..
ifdef USE_OUR_AS
AS = $(ROOT)/cmd/as/as
else
AS = $(CROSS)gcc
endif
AR = $(CROSS)ar

CFLAGS = -I$(CC_INCLUDE) -Iinclude
ASFLAGS = -g -mcpu=cortex-a57

OBJ = crt0.o syscall.o stdio.o stdio_file.o string.o ctype.o test.o malloc.o stdlib.o errno.o libgen.o time.o

all: libc.a

libc.a: $(OBJ)
	$(AR) rcs $@ $^

ifdef USE_OUR_AS
%.o: %.S
	$(CROSS)gcc -E -x assembler-with-cpp $< | $(AS) -o $@
else
%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@
endif

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libc.a

.PHONY: all clean
