ROOT = $(CURDIR)

MKFS = $(ROOT)/tools/mkfs/mkfs
MKRAMFS = $(ROOT)/tools/mkramfs/mkramfs
LIBC = $(ROOT)/libc/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include
CC = $(ROOT)/tools/cc/chibicc
CC_INCLUDE = $(ROOT)/cmd/cc/include
AS = $(ROOT)/tools/as/as
LD = $(ROOT)/tools/ld/ld

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

$(AS):
	$(MAKE) -C tools/as

$(LD):
	$(MAKE) -C tools/ld

cmd: $(LIBC) tools/cc/chibicc $(AS) $(LD)
	$(MAKE) -C cmd CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)

initramfs-test.bin: $(MKRAMFS) cmd
	$(MKRAMFS) $@ $(addprefix cmd/,$(PROGS))

kernel/kernel.bin:
	$(MAKE) -C kernel kernel.bin

kernel/kernel-test.bin:
	$(MAKE) -C kernel kernel-test.bin

disk.img: $(MKFS) cmd
	$(MKFS) $@ -s 8192 \
		:dir:/dev \
		:dir:/bin \
		:dir:/lib \
		:dir:/tmp \
		:dir:/src \
		:dir:/src/libc \
		:dir:/src/ld \
		:dir:/src/cc \
		:dir:/src/cc/include \
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
		cmd/cmp.elf:/bin/cmp \
		cmd/touch.elf:/bin/touch \
		cmd/wc.elf:/bin/wc \
		cmd/head.elf:/bin/head \
		cmd/grep.elf:/bin/grep \
		cmd/sed.elf:/bin/sed \
		cmd/ps.elf:/bin/ps \
		cmd/kill.elf:/bin/kill \
		cmd/sleep.elf:/bin/sleep \
		cmd/as.elf:/bin/as \
		cmd/ld.elf:/bin/ld \
		cmd/cc.elf:/bin/cc \
		cmd/buildcc.elf:/bin/buildcc \
		libc/libc.a:/lib/libc.a \
		cmd/cat/cat.c:/src/cat.c \
		cmd/cp/cp.c:/src/cp.c \
		cmd/echo/echo.c:/src/echo.c \
		cmd/grep/grep.c:/src/grep.c \
		cmd/sed/sed.c:/src/sed.c \
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
		libc/include/assert.h:/src/libc/assert.h \
		libc/include/ctype.h:/src/libc/ctype.h \
		libc/include/errno.h:/src/libc/errno.h \
		libc/include/fcntl.h:/src/libc/fcntl.h \
		libc/include/inttypes.h:/src/libc/inttypes.h \
		libc/include/libgen.h:/src/libc/libgen.h \
		libc/include/signal.h:/src/libc/signal.h \
		libc/include/stdarg.h:/src/libc/stdarg.h \
		libc/include/stdio.h:/src/libc/stdio.h \
		libc/include/stdlib.h:/src/libc/stdlib.h \
		libc/include/string.h:/src/libc/string.h \
		libc/include/strings.h:/src/libc/strings.h \
		libc/include/time.h:/src/libc/time.h \
		libc/include/unistd.h:/src/libc/unistd.h \
		:dir:/src/libc/sys \
		libc/include/sys/mman.h:/src/libc/sys/mman.h \
		libc/include/sys/stat.h:/src/libc/sys/stat.h \
		libc/include/sys/wait.h:/src/libc/sys/wait.h \
		cmd/ld/hello.S:/src/ld/hello.S \
		cmd/cc/main.c:/src/cc/main.c \
		cmd/cc/tokenize.c:/src/cc/tokenize.c \
		cmd/cc/preprocess.c:/src/cc/preprocess.c \
		cmd/cc/parse.c:/src/cc/parse.c \
		cmd/cc/type.c:/src/cc/type.c \
		cmd/cc/codegen.c:/src/cc/codegen.c \
		cmd/cc/unicode.c:/src/cc/unicode.c \
		cmd/cc/strings.c:/src/cc/strings.c \
		cmd/cc/hashmap.c:/src/cc/hashmap.c \
		cmd/cc/chibicc.h:/src/cc/chibicc.h \
		cmd/cc/hello.c:/src/cc/hello.c \
		cmd/cc/include/assert.h:/src/cc/include/assert.h \
		cmd/cc/include/float.h:/src/cc/include/float.h \
		cmd/cc/include/stdalign.h:/src/cc/include/stdalign.h \
		cmd/cc/include/stdarg.h:/src/cc/include/stdarg.h \
		cmd/cc/include/stdatomic.h:/src/cc/include/stdatomic.h \
		cmd/cc/include/stdbool.h:/src/cc/include/stdbool.h \
		cmd/cc/include/stddef.h:/src/cc/include/stddef.h \
		cmd/cc/include/stdint.h:/src/cc/include/stdint.h \
		cmd/cc/include/stdnoreturn.h:/src/cc/include/stdnoreturn.h

disk-test.img: $(MKFS) cmd
	$(MKFS) $@ -s 2048 \
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
	clang-format -i cmd/cc/*.c cmd/cc/*.h cmd/cc/include/*.h
	clang-format -i tools/mkfs/*.c tools/mkramfs/*.c
	clang-format -i tools/cc/*.c tools/cc/*.h
