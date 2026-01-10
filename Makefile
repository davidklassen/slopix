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

# Source files - include ALL test files in every build
OBJS = boot.o main.o uart.o printf.o exceptions.o gic.o timer.o interrupts.o pmm.o mmu.o process.o scheduler.o kernel_state.o \
       tests/test_pmm.o tests/test_processes.o tests/test_mmu_registers.o tests/test_mmu_tables.o tests/test_mmu_enable.o tests/test_mmu_ttbr1.o

# Single target
TARGET = slopix.elf

# Default target - normal kernel (no TEST_BUILD flag)
all: clean-main $(TARGET)

# Test target - rebuild with TEST_BUILD flag
test: CFLAGS += -DTEST_BUILD
test: clean-main $(TARGET)

$(TARGET): $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

# Clean only main.o to force rebuild with/without TEST_BUILD flag
clean-main:
	@rm -f main.o

clean:
	rm -f $(OBJS) $(TARGET) *.o tests/*.o

run: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TARGET)

run-test: test
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TARGET)

.PHONY: all clean run test run-test clean-main
