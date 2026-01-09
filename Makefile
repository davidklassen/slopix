# Toolchain
CROSS_COMPILE ?= aarch64-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Flags
CFLAGS = -Wall -Wextra -ffreestanding -nostdlib -O2 -mcpu=cortex-a53
ASFLAGS = -mcpu=cortex-a53
LDFLAGS = -T linker.ld -nostdlib

# Source files
OBJS = boot.o main.o uart.o printf.o

# Target
TARGET = slopix.elf

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TARGET)

.PHONY: all clean run
