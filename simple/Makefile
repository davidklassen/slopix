CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57
ASFLAGS = -g -mcpu=cortex-a57

OBJS = boot.o tables.o uart.o main.o

all: kernel.elf

kernel.elf: $(OBJS) linker.ld
	$(LD) -nostdlib -T linker.ld -o $@ $(OBJS)

boot.o: boot.S
	$(CC) $(ASFLAGS) -c $< -o $@

tables.o: tables.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: kernel.elf
	qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic -kernel kernel.elf

clean:
	rm -f *.o kernel.elf

.PHONY: all run clean
