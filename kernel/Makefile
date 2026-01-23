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
	bio.o \
	fs.o \
	file.o \
	pipe.o \
	console.o \
	disk.o \
	string.o \
	sync.o \
	dtb.o \
	cmdline.o \
	initramfs_data.o

TEST_OBJ = \
	tests/test.o \
	tests/test_uart.o \
	tests/test_kprintf.o \
	tests/test_exception.o \
	tests/test_timer.o \
	tests/test_vmm.o \
	tests/test_pmm.o \
	tests/test_initramfs.o \
	tests/test_virtio.o \
	tests/test_bio.o \
	tests/test_fs.o \
	tests/test_file.o \
	tests/test_console.o \
	tests/test_pipe.o \
	tests/test_string.o \
	tests/test_sync.o

KERNEL = kernel.elf
KERNEL_BIN = kernel.bin

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU = $(QEMU_BASE)
QEMU_DISK = $(QEMU_BASE) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) -drive file=test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

.PHONY: all clean run debug test

all: $(KERNEL)

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

$(KERNEL_BIN): $(KERNEL)
	$(OBJCOPY) -O binary $< $@

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

../cmd/initramfs.bin:
	$(MAKE) -C ../cmd initramfs.bin

initramfs_data.o: ../cmd/initramfs.bin
	$(OBJCOPY) -I binary -O elf64-littleaarch64 -B aarch64 \
		--rename-section .data=.initramfs \
		$< $@

USER_PROGS = ../cmd/init.elf ../cmd/echo.elf ../cmd/shell.elf ../cmd/shutdown.elf \
	../cmd/true.elf ../cmd/false.elf ../cmd/ticker.elf ../cmd/cursor_blink.elf \
	../cmd/cat.elf ../cmd/ls.elf ../cmd/mkdir.elf ../cmd/rm.elf ../cmd/cp.elf \
	../cmd/mv.elf ../cmd/touch.elf ../cmd/wc.elf ../cmd/head.elf ../cmd/grep.elf

$(USER_PROGS):
	$(MAKE) -C ../cmd

disk.img: ../tools/mkfs $(USER_PROGS)
	../tools/mkfs $@ -s 2048 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		../cmd/init.elf:/init \
		../cmd/echo.elf:/echo \
		../cmd/shell.elf:/shell \
		../cmd/shutdown.elf:/shutdown \
		../cmd/true.elf:/true \
		../cmd/false.elf:/false \
		../cmd/ticker.elf:/ticker \
		../cmd/cursor_blink.elf:/cursor_blink \
		../cmd/cat.elf:/cat \
		../cmd/ls.elf:/ls \
		../cmd/mkdir.elf:/mkdir \
		../cmd/rm.elf:/rm \
		../cmd/cp.elf:/cp \
		../cmd/mv.elf:/mv \
		../cmd/touch.elf:/touch \
		../cmd/wc.elf:/wc \
		../cmd/head.elf:/head \
		../cmd/grep.elf:/grep

run: clean $(KERNEL_BIN) disk.img
	$(QEMU_DISK) -kernel $(KERNEL_BIN) -append "init=/init"

debug: clean $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -s -S

../tools/mkfs:
	$(MAKE) -C ../tools mkfs

test.img: ../tools/mkfs ../testdata/hello.txt ../testdata/large.txt $(USER_PROGS)
	../tools/mkfs $@ -s 1024 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		../testdata/hello.txt:/hello \
		../testdata/large.txt:/large \
		../cmd/true.elf:/true \
		../cmd/false.elf:/false

test: CFLAGS += -DRUN_TESTS
test: clean $(OBJ) $(TEST_OBJ) test.img
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ) $(TEST_OBJ)
	$(OBJCOPY) -O binary $(KERNEL) $(KERNEL_BIN)
	$(QEMU_TEST) -kernel $(KERNEL_BIN) -append "init=initramfs:tests"

clean:
	rm -f *.o *.elf *.bin tests/*.o tests/*.elf disk.img test.img
	$(MAKE) -C ../cmd clean
