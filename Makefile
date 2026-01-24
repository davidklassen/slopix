ROOT = $(CURDIR)

MKFS = $(ROOT)/tools/mkfs
MKRAMFS = $(ROOT)/tools/mkramfs
LIBC = $(ROOT)/libc/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include
INITRAMFS = $(ROOT)/cmd/initramfs.bin

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_DISK = $(QEMU_BASE) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) -drive file=test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

.PHONY: all clean run test tidy

all: kernel/kernel.bin

$(MKFS) $(MKRAMFS):
	$(MAKE) -C tools

$(LIBC):
	$(MAKE) -C libc

$(INITRAMFS): $(LIBC) $(MKRAMFS)
	$(MAKE) -C cmd LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) MKRAMFS=$(MKRAMFS)

kernel/kernel.bin: $(INITRAMFS)
	$(MAKE) -C kernel kernel.bin

kernel/kernel-test.bin: $(INITRAMFS)
	$(MAKE) -C kernel kernel-test.bin

disk.img: $(MKFS) $(INITRAMFS)
	$(MKFS) $@ -s 2048 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		$(ROOT)/cmd/init.elf:/init \
		$(ROOT)/cmd/echo.elf:/echo \
		$(ROOT)/cmd/shell.elf:/shell \
		$(ROOT)/cmd/shutdown.elf:/shutdown \
		$(ROOT)/cmd/true.elf:/true \
		$(ROOT)/cmd/false.elf:/false \
		$(ROOT)/cmd/ticker.elf:/ticker \
		$(ROOT)/cmd/cursor_blink.elf:/cursor_blink \
		$(ROOT)/cmd/cat.elf:/cat \
		$(ROOT)/cmd/ls.elf:/ls \
		$(ROOT)/cmd/mkdir.elf:/mkdir \
		$(ROOT)/cmd/rm.elf:/rm \
		$(ROOT)/cmd/cp.elf:/cp \
		$(ROOT)/cmd/mv.elf:/mv \
		$(ROOT)/cmd/touch.elf:/touch \
		$(ROOT)/cmd/wc.elf:/wc \
		$(ROOT)/cmd/head.elf:/head \
		$(ROOT)/cmd/grep.elf:/grep \
		$(ROOT)/cmd/ps.elf:/ps \
		$(ROOT)/cmd/kill.elf:/kill

test.img: $(MKFS) $(INITRAMFS)
	$(MKFS) $@ -s 1024 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		$(ROOT)/testdata/hello.txt:/hello \
		$(ROOT)/testdata/large.txt:/large \
		$(ROOT)/cmd/true.elf:/true \
		$(ROOT)/cmd/false.elf:/false

run: clean disk.img kernel/kernel.bin
	$(QEMU_DISK) -kernel kernel/kernel.bin -append "init=/init"

test: clean test.img kernel/kernel-test.bin
	$(QEMU_TEST) -kernel kernel/kernel-test.bin -append "init=initramfs:tests"

clean:
	$(MAKE) -C tools clean
	$(MAKE) -C libc clean
	$(MAKE) -C cmd clean
	$(MAKE) -C kernel clean
	rm -f disk.img test.img

tidy:
	clang-format -i kernel/*.c kernel/*.h kernel/tests/*.c kernel/tests/*.h
	clang-format -i libc/*.c libc/include/*.h
	clang-format -i cmd/*/*.c
	clang-format -i tools/*.c
