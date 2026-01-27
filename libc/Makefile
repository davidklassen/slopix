CROSS = aarch64-elf-
AS = $(CROSS)gcc
AR = $(CROSS)ar

CFLAGS = -I$(CC_INCLUDE) -Iinclude
ASFLAGS = -g -mcpu=cortex-a57

OBJ = crt0.o syscall.o stdio.o stdio_file.o string.o ctype.o test.o malloc.o stdlib.o errno.o

all: libc.a

libc.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.S
	$(AS) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libc.a

.PHONY: all clean
