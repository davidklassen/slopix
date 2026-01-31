ROOT = $(CURDIR)

MKFS = $(ROOT)/tools/mkfs/mkfs
MKRAMFS = $(ROOT)/tools/mkramfs/mkramfs
LIBC = $(ROOT)/libc/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include
CC = $(ROOT)/tools/cc/chibicc
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
	$(MAKE) -C libc CC=$(CC)

tools/cc/chibicc:
	$(MAKE) -C tools/cc

$(AS):
	$(MAKE) -C tools/as

$(LD):
	$(MAKE) -C tools/ld

cmd: $(LIBC) tools/cc/chibicc $(AS) $(LD)
	$(MAKE) -C cmd CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE)

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
		:dir:/src/cmd \
		:dir:/src/cmd/cat \
		:dir:/src/cmd/cp \
		:dir:/src/cmd/echo \
		:dir:/src/cmd/grep \
		:dir:/src/cmd/sed \
		:dir:/src/cmd/head \
		:dir:/src/cmd/kill \
		:dir:/src/cmd/ls \
		:dir:/src/cmd/mkdir \
		:dir:/src/cmd/mv \
		:dir:/src/cmd/ps \
		:dir:/src/cmd/rm \
		:dir:/src/cmd/sleep \
		:dir:/src/cmd/touch \
		:dir:/src/cmd/wc \
		:dir:/src/cmd/ld \
		:dir:/src/cmd/cc \
		:dir:/src/cmd/buildcc \
		:dir:/src/cmd/buildlibc \
		:dir:/src/cmd/ar \
		:dir:/src/libc \
		:dir:/src/libc/include \
		:dir:/src/libc/include/sys \
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
		cmd/ar.elf:/bin/ar \
		cmd/buildcc.elf:/bin/buildcc \
		cmd/buildlibc.elf:/bin/buildlibc \
		cmd/ed.elf:/bin/ed \
		libc/libc.a:/lib/libc.a \
		cmd/cat/cat.c:/src/cmd/cat/cat.c \
		cmd/cp/cp.c:/src/cmd/cp/cp.c \
		cmd/echo/echo.c:/src/cmd/echo/echo.c \
		cmd/grep/grep.c:/src/cmd/grep/grep.c \
		cmd/sed/sed.c:/src/cmd/sed/sed.c \
		cmd/head/head.c:/src/cmd/head/head.c \
		cmd/kill/kill.c:/src/cmd/kill/kill.c \
		cmd/ls/ls.c:/src/cmd/ls/ls.c \
		cmd/mkdir/mkdir.c:/src/cmd/mkdir/mkdir.c \
		cmd/mv/mv.c:/src/cmd/mv/mv.c \
		cmd/ps/ps.c:/src/cmd/ps/ps.c \
		cmd/rm/rm.c:/src/cmd/rm/rm.c \
		cmd/sleep/sleep.c:/src/cmd/sleep/sleep.c \
		cmd/touch/touch.c:/src/cmd/touch/touch.c \
		cmd/wc/wc.c:/src/cmd/wc/wc.c \
		cmd/ld/hello.S:/src/cmd/ld/hello.S \
		cmd/cc/main.c:/src/cmd/cc/main.c \
		cmd/cc/tokenize.c:/src/cmd/cc/tokenize.c \
		cmd/cc/preprocess.c:/src/cmd/cc/preprocess.c \
		cmd/cc/parse.c:/src/cmd/cc/parse.c \
		cmd/cc/type.c:/src/cmd/cc/type.c \
		cmd/cc/codegen.c:/src/cmd/cc/codegen.c \
		cmd/cc/unicode.c:/src/cmd/cc/unicode.c \
		cmd/cc/strings.c:/src/cmd/cc/strings.c \
		cmd/cc/hashmap.c:/src/cmd/cc/hashmap.c \
		cmd/cc/chibicc.h:/src/cmd/cc/chibicc.h \
		cmd/cc/hello.c:/src/cmd/cc/hello.c \
		cmd/buildcc/buildcc.c:/src/cmd/buildcc/buildcc.c \
		cmd/buildlibc/buildlibc.c:/src/cmd/buildlibc/buildlibc.c \
		cmd/ar/ar.c:/src/cmd/ar/ar.c \
		libc/crt0.S:/src/libc/crt0.S \
		libc/ctype.c:/src/libc/ctype.c \
		libc/errno.c:/src/libc/errno.c \
		libc/libgen.c:/src/libc/libgen.c \
		libc/malloc.c:/src/libc/malloc.c \
		libc/stdio.c:/src/libc/stdio.c \
		libc/stdio_file.c:/src/libc/stdio_file.c \
		libc/stdlib.c:/src/libc/stdlib.c \
		libc/string.c:/src/libc/string.c \
		libc/syscall.S:/src/libc/syscall.S \
		libc/test.c:/src/libc/test.c \
		libc/time.c:/src/libc/time.c \
		libc/include/assert.h:/src/libc/include/assert.h \
		libc/include/ctype.h:/src/libc/include/ctype.h \
		libc/include/errno.h:/src/libc/include/errno.h \
		libc/include/fcntl.h:/src/libc/include/fcntl.h \
		libc/include/float.h:/src/libc/include/float.h \
		libc/include/inttypes.h:/src/libc/include/inttypes.h \
		libc/include/libgen.h:/src/libc/include/libgen.h \
		libc/include/signal.h:/src/libc/include/signal.h \
		libc/include/stdalign.h:/src/libc/include/stdalign.h \
		libc/include/stdarg.h:/src/libc/include/stdarg.h \
		libc/include/stdatomic.h:/src/libc/include/stdatomic.h \
		libc/include/stdbool.h:/src/libc/include/stdbool.h \
		libc/include/stddef.h:/src/libc/include/stddef.h \
		libc/include/stdint.h:/src/libc/include/stdint.h \
		libc/include/stdnoreturn.h:/src/libc/include/stdnoreturn.h \
		libc/include/stdio.h:/src/libc/include/stdio.h \
		libc/include/stdlib.h:/src/libc/include/stdlib.h \
		libc/include/string.h:/src/libc/include/string.h \
		libc/include/strings.h:/src/libc/include/strings.h \
		libc/include/test.h:/src/libc/include/test.h \
		libc/include/time.h:/src/libc/include/time.h \
		libc/include/unistd.h:/src/libc/include/unistd.h \
		libc/include/sys/mman.h:/src/libc/include/sys/mman.h \
		libc/include/sys/stat.h:/src/libc/include/sys/stat.h \
		libc/include/sys/wait.h:/src/libc/include/sys/wait.h

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
	clang-format -i cmd/cc/*.c cmd/cc/*.h
	clang-format -i tools/mkfs/*.c tools/mkramfs/*.c
	clang-format -i tools/cc/*.c tools/cc/*.h
