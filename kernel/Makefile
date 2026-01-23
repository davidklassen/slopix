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
	vmm.o \
	pmm.o \
	proc.o \
	switch.o \
	syscall.o \
	elf.o \
	init.o \
	initramfs.o \
	virtio.o \
	initramfs_data.o

TEST_OBJ = \
	tests/test.o \
	tests/test_uart.o \
	tests/test_kprintf.o \
	tests/test_exception.o \
	tests/test_timer.o \
	tests/test_vmm.o \
	tests/test_pmm.o \
	tests/test_proc.o \
	tests/test_vmm_user.o \
	tests/test_elf.o \
	tests/test_initramfs.o \
	tests/test_virtio.o

KERNEL = kernel.elf

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU = $(QEMU_BASE)
QEMU_DISK = $(QEMU_BASE) -drive file=test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

.PHONY: all clean run debug test

all: $(KERNEL)

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

../cmd/initramfs.bin:
	$(MAKE) -C ../cmd initramfs.bin

../cmd/initramfs-test.bin:
	$(MAKE) -C ../cmd initramfs-test.bin

initramfs_data.o: ../cmd/initramfs.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.initramfs \
		$< $@

initramfs_data_test.o: ../cmd/initramfs-test.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.initramfs \
		$< $@

run: clean $(KERNEL) test.img
	$(QEMU_DISK) -kernel $(KERNEL)

debug: clean $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -s -S

test.img:
	dd if=/dev/zero of=$@ bs=1M count=1

test: CFLAGS += -DRUN_TESTS
test: clean ../cmd/initramfs-test.bin initramfs_data_test.o $(filter-out initramfs_data.o,$(OBJ)) $(TEST_OBJ) test.img
	$(LD) $(LDFLAGS) -o $(KERNEL) $(filter-out initramfs_data.o,$(OBJ)) initramfs_data_test.o $(TEST_OBJ)
	$(QEMU_DISK) -kernel $(KERNEL)

clean:
	rm -f *.o *.elf tests/*.o tests/*.elf
