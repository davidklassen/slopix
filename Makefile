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
OBJS = boot.o main.o uart.o printf.o exceptions.o gic.o timer.o interrupts.o pmm.o process.o scheduler.o
TEST_OBJS = boot.o tests/test_main.o tests/test_pmm.o tests/test_processes.o uart.o printf.o exceptions.o gic.o timer.o interrupts.o pmm.o process.o scheduler.o

# Targets
TARGET = slopix.elf
TEST_TARGET = slopix-test.elf

all: $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(TEST_TARGET): $(TEST_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

test: $(TEST_TARGET)

run-test: $(TEST_TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TEST_TARGET)

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_TARGET) *.o tests/*.o

run: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TARGET)

.PHONY: all clean run test run-test
