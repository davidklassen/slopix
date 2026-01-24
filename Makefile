ROOT = $(CURDIR)

MKFS = $(ROOT)/tools/mkfs
MKRAMFS = $(ROOT)/tools/mkramfs
LIBC = $(ROOT)/libc/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_DISK = $(QEMU_BASE) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) -drive file=disk-test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

PROGS = init.elf shell.elf cursor_blink.elf echo.elf ticker.elf shutdown.elf \
	true.elf false.elf cat.elf ls.elf mkdir.elf rm.elf cp.elf mv.elf \
	touch.elf wc.elf head.elf grep.elf ps.elf kill.elf sleep.elf tests.elf

.PHONY: all clean run test tidy cmd

all: kernel/kernel.bin

$(MKFS) $(MKRAMFS):
	$(MAKE) -C tools

$(LIBC):
	$(MAKE) -C libc

cmd: $(LIBC)
	$(MAKE) -C cmd LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE)

initramfs-test.bin: $(MKRAMFS) cmd
	$(MKRAMFS) $@ $(addprefix cmd/,$(PROGS))

kernel/kernel.bin:
	$(MAKE) -C kernel kernel.bin

kernel/kernel-test.bin:
	$(MAKE) -C kernel kernel-test.bin

disk.img: $(MKFS) cmd
	$(MKFS) $@ -s 2048 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		cmd/init.elf:/init \
		cmd/echo.elf:/echo \
		cmd/shell.elf:/shell \
		cmd/shutdown.elf:/shutdown \
		cmd/true.elf:/true \
		cmd/false.elf:/false \
		cmd/ticker.elf:/ticker \
		cmd/cursor_blink.elf:/cursor_blink \
		cmd/cat.elf:/cat \
		cmd/ls.elf:/ls \
		cmd/mkdir.elf:/mkdir \
		cmd/rm.elf:/rm \
		cmd/cp.elf:/cp \
		cmd/mv.elf:/mv \
		cmd/touch.elf:/touch \
		cmd/wc.elf:/wc \
		cmd/head.elf:/head \
		cmd/grep.elf:/grep \
		cmd/ps.elf:/ps \
		cmd/kill.elf:/kill

disk-test.img: $(MKFS) cmd
	$(MKFS) $@ -s 1024 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		testdata/hello.txt:/hello \
		testdata/large.txt:/large \
		cmd/true.elf:/true \
		cmd/false.elf:/false

run: clean disk.img kernel/kernel.bin
	$(QEMU_DISK) -kernel kernel/kernel.bin -append "init=/init"

test: clean disk-test.img kernel/kernel-test.bin initramfs-test.bin
	$(QEMU_TEST) -kernel kernel/kernel-test.bin -initrd initramfs-test.bin -append "init=initramfs:tests"

clean:
	$(MAKE) -C tools clean
	$(MAKE) -C libc clean
	$(MAKE) -C cmd clean
	$(MAKE) -C kernel clean
	rm -f disk.img disk-test.img initramfs-test.bin

tidy:
	clang-format -i kernel/*.c kernel/*.h kernel/tests/*.c kernel/tests/*.h
	clang-format -i libc/*.c libc/include/*.h
	clang-format -i cmd/*/*.c
	clang-format -i tools/*.c
