CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I.
ASFLAGS = -g -mcpu=cortex-a57
LDFLAGS = -nostdlib -T linker.ld

OBJ = \
	boot.o \
	tables.o \
	kernel.o \
	uart.o \
	psci.o \
	kprintf.o \
	vectors.o \
	exception.o \
	gic.o \
	timer.o \
	prompt.o \
	mmu.o \
	pmem.o \
	proc.o \
	switch.o \
	syscall.o \
	elf.o \
	initramfs.o \
	initramfs_data.o

TEST_OBJ = \
	tests/test.o \
	tests/test_uart.o \
	tests/test_kprintf.o \
	tests/test_exception.o \
	tests/test_timer.o \
	tests/test_mmu.o \
	tests/test_pmem.o \
	tests/test_proc.o \
	tests/test_uvm.o \
	tests/test_elf.o \
	tests/test_initramfs.o

KERNEL = kernel.elf

QEMU = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic

.PHONY: all clean run test tidy

all: $(KERNEL)

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

cmd/initramfs.bin:
	$(MAKE) -C cmd initramfs.bin

initramfs_data.o: cmd/initramfs.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.initramfs \
		$< $@

run: clean $(KERNEL)
	$(QEMU) -kernel $(KERNEL)

test: CFLAGS += -DRUN_TESTS
test: clean $(OBJ) $(TEST_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ) $(TEST_OBJ)
	$(QEMU) -kernel $(KERNEL)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(KERNEL)
	$(MAKE) -C cmd clean

tidy:
	clang-format -i *.c *.h tests/*.c tests/*.h cmd/*.c cmd/*.h
