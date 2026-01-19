CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I.
ASFLAGS = -g -mcpu=cortex-a57
LDFLAGS = -nostdlib -T link.ld

COMMON_OBJ = crt0.o syscall.o libc.o

PROGS = hello.elf

all: $(PROGS)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.elf: %.o $(COMMON_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(COMMON_OBJ) $<

clean:
	rm -f *.o *.elf

.PHONY: all clean
