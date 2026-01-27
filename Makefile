ROOT = $(CURDIR)

MKFS = $(ROOT)/tools/mkfs/mkfs
MKRAMFS = $(ROOT)/tools/mkramfs/mkramfs
LIBC = $(ROOT)/libc/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include
CC = $(ROOT)/tools/cc/chibicc
CC_INCLUDE = $(ROOT)/tools/cc/include

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

$(LIBC): tools/cc/chibicc
	$(MAKE) -C libc CC=$(CC) CC_INCLUDE=$(CC_INCLUDE)

tools/cc/chibicc:
	$(MAKE) -C tools/cc

cmd: $(LIBC) tools/cc/chibicc
	$(MAKE) -C cmd CC=$(CC) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)

initramfs-test.bin: $(MKRAMFS) cmd
	$(MKRAMFS) $@ $(addprefix cmd/,$(PROGS))

kernel/kernel.bin:
	$(MAKE) -C kernel kernel.bin

kernel/kernel-test.bin:
	$(MAKE) -C kernel kernel-test.bin

disk.img: $(MKFS) cmd
	$(MKFS) $@ -s 4096 \
		:dir:/dev \
		:dir:/bin \
		:dir:/src \
		:dir:/src/libc \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		cmd/init.elf:/init \
		cmd/shell.elf:/bin/shell \
		cmd/echo.elf:/bin/echo \
		cmd/shutdown.elf:/bin/shutdown \
		cmd/true.elf:/bin/true \
		cmd/false.elf:/bin/false \
		cmd/ticker.elf:/bin/ticker \
		cmd/cursor_blink.elf:/bin/cursor_blink \
		cmd/cat.elf:/bin/cat \
		cmd/ls.elf:/bin/ls \
		cmd/mkdir.elf:/bin/mkdir \
		cmd/rm.elf:/bin/rm \
		cmd/cp.elf:/bin/cp \
		cmd/mv.elf:/bin/mv \
		cmd/touch.elf:/bin/touch \
		cmd/wc.elf:/bin/wc \
		cmd/head.elf:/bin/head \
		cmd/grep.elf:/bin/grep \
		cmd/ps.elf:/bin/ps \
		cmd/kill.elf:/bin/kill \
		cmd/sleep.elf:/bin/sleep \
		cmd/cat/cat.c:/src/cat.c \
		cmd/cp/cp.c:/src/cp.c \
		cmd/echo/echo.c:/src/echo.c \
		cmd/grep/grep.c:/src/grep.c \
		cmd/head/head.c:/src/head.c \
		cmd/kill/kill.c:/src/kill.c \
		cmd/ls/ls.c:/src/ls.c \
		cmd/mkdir/mkdir.c:/src/mkdir.c \
		cmd/mv/mv.c:/src/mv.c \
		cmd/ps/ps.c:/src/ps.c \
		cmd/rm/rm.c:/src/rm.c \
		cmd/sleep/sleep.c:/src/sleep.c \
		cmd/touch/touch.c:/src/touch.c \
		cmd/wc/wc.c:/src/wc.c \
		libc/ctype.c:/src/libc/ctype.c \
		libc/stdio.c:/src/libc/stdio.c \
		libc/string.c:/src/libc/string.c \
		libc/syscall.S:/src/libc/syscall.S \
		libc/include/ctype.h:/src/libc/ctype.h \
		libc/include/fcntl.h:/src/libc/fcntl.h \
		libc/include/signal.h:/src/libc/signal.h \
		libc/include/stdio.h:/src/libc/stdio.h \
		libc/include/string.h:/src/libc/string.h \
		libc/include/unistd.h:/src/libc/unistd.h

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
	clang-format -i cmd/tests/*.c
	clang-format -i tools/mkfs/*.c tools/mkramfs/*.c
	clang-format -i tools/cc/*.c tools/cc/*.h tools/cc/include/*.h
