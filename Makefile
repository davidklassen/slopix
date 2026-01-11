# Toolchain
CROSS_COMPILE ?= aarch64-elf-
CC = $(CROSS_COMPILE)gcc
AS = $(CROSS_COMPILE)as
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Flags
CFLAGS = -Wall -Wextra -ffreestanding -nostdlib -O2 -mcpu=cortex-a53 -mgeneral-regs-only
ASFLAGS = -mcpu=cortex-a53
LDFLAGS = -T linker.ld -nostdlib

# Base kernel objects (always compiled)
OBJS = boot.o main.o uart.o printf.o exceptions.o gic.o timer.o interrupts.o pmm.o mmu.o process.o scheduler.o kernel_state.o

# Test objects (always compiled)
TEST_OBJS = tests/test_globals.o tests/test_pmm.o tests/test_processes.o tests/test_mmu_registers.o tests/test_mmu_tables.o tests/test_mmu_enable.o tests/test_mmu_ttbr1.o tests/test_higher_half.o tests/test_sync_exception.o tests/test_process_context_fields.o tests/test_process_context_init.o tests/test_pstate_constants.o tests/test_process_el_detection.o tests/test_context_frame_size.o tests/test_dual_stack_foundation.o tests/test_pte_bits.o

# All objects to link
ALL_OBJS = $(OBJS) $(TEST_OBJS)

# Single target
TARGET = slopix.elf

# Default target - normal kernel (no TEST_BUILD flag)
all: clean-main $(TARGET)

# Test target - rebuild with TEST_BUILD flag
test: CFLAGS += -DTEST_BUILD
test: clean-main $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(ASFLAGS) -c -o $@ $<

# Clean only main.o to force rebuild with/without TEST_BUILD flag
clean-main:
	@rm -f main.o

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) *.o tests/*.o

run: $(TARGET)
	qemu-system-aarch64 -M virt -cpu cortex-a53 -nographic -kernel $(TARGET)

run-test: test
	qemu-system-aarch64 -M virt -cpu cortex-a53 -semihosting -nographic -kernel $(TARGET)

.PHONY: all clean run test run-test clean-main
