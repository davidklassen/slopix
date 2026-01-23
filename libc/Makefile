CROSS = aarch64-elf-
CC = $(CROSS)gcc
AR = $(CROSS)ar

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -Iinclude
ASFLAGS = -g -mcpu=cortex-a57

OBJ = crt0.o syscall.o stdio.o string.o ctype.o test.o

all: libc.a

libc.a: $(OBJ)
	$(AR) rcs $@ $^

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o libc.a

.PHONY: all clean
