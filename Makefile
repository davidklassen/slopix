ROOT = $(CURDIR)

# Host tools built in .bin/
HOSTCC ?= cc
HOST_CFLAGS = -std=c11 -g -Wall -Wextra -Werror -O0

BUILD = $(ROOT)/.bin/build
MKFS = $(ROOT)/.bin/mkfs
MKRAMFS = $(ROOT)/.bin/mkramfs
LIBC = $(ROOT)/.build/out/lib/libc.a
LIBC_INCLUDE = $(ROOT)/libc/include
CC = $(ROOT)/.bin/cc
AS = $(ROOT)/.bin/as
LD = $(ROOT)/.bin/ld

# Environment for cross-compiler
export CC_INCLUDE_PATH = $(LIBC_INCLUDE)
export CC_AS = $(AS)
export CC_LD = $(LD)
export CC_LIBC = $(LIBC)

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_DISK = $(QEMU_BASE) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) -drive file=disk-test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

PROGS = init.elf shell.elf cursor_blink.elf echo.elf ticker.elf shutdown.elf \
	true.elf false.elf cat.elf ls.elf mkdir.elf rm.elf cp.elf mv.elf \
	touch.elf wc.elf head.elf grep.elf ps.elf kill.elf sleep.elf tests.elf

.PHONY: all clean run test tidy cmd userspace

all: kernel/kernel.bin

# Step 1: Bootstrap build tool with system compiler
.bin/build: cmd/build/main.c lib/build.h | .bin
	$(HOSTCC) -I lib $(HOST_CFLAGS) -o $@ cmd/build/main.c

# Step 2: Build host cross-toolchain using build tool
# LD=cc is needed because host ld doesn't link libc implicitly
.bin/cc: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/cc

.bin/as: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/as

.bin/ld: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/ld

.bin/ar: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/ar

.bin/mkfs: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/mkfs

.bin/mkramfs: .bin/build | .bin
	LD=$(HOSTCC) $(BUILD) --prefix=.bin cmd/mkramfs

# Directory creation
.bin:
	mkdir -p $@

# Step 3: Build userspace (cross-compile libc + all commands)
# Uses root build.c which handles artifact bubbling to .build/out/
userspace: .bin/cc .bin/as .bin/ld .bin/ar
	BUILD=$(BUILD) CC=$(CC) AS=$(AS) LD=$(LD) AR=$(ROOT)/.bin/ar \
	INCLUDE_PATH=$(LIBC_INCLUDE) LIB_PATH=$(ROOT)/.build/out/lib \
	BUILD_INCLUDE=$(ROOT)/lib \
	$(BUILD)

# Aliases for backwards compatibility
$(LIBC): userspace
cmd: userspace

initramfs-test.bin: .bin/mkramfs cmd
	$(MKRAMFS) $@ \
		.build/out/bin/init .build/out/bin/shell .build/out/bin/cursor_blink \
		.build/out/bin/echo .build/out/bin/ticker .build/out/bin/shutdown \
		.build/out/bin/true .build/out/bin/false .build/out/bin/cat .build/out/bin/ls \
		.build/out/bin/mkdir .build/out/bin/rm .build/out/bin/cp .build/out/bin/mv \
		.build/out/bin/touch .build/out/bin/wc .build/out/bin/head .build/out/bin/grep \
		.build/out/bin/ps .build/out/bin/kill .build/out/bin/sleep .build/out/bin/tests

kernel/kernel.bin:
	$(MAKE) -C kernel kernel.bin

kernel/kernel-test.bin:
	$(MAKE) -C kernel kernel-test.bin

disk.img: .bin/mkfs cmd
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
		.build/out/bin/init:/init \
		.build/out/bin/shell:/bin/shell \
		.build/out/bin/echo:/bin/echo \
		.build/out/bin/shutdown:/bin/shutdown \
		.build/out/bin/true:/bin/true \
		.build/out/bin/false:/bin/false \
		.build/out/bin/ticker:/bin/ticker \
		.build/out/bin/cursor_blink:/bin/cursor_blink \
		.build/out/bin/cat:/bin/cat \
		.build/out/bin/ls:/bin/ls \
		.build/out/bin/mkdir:/bin/mkdir \
		.build/out/bin/rm:/bin/rm \
		.build/out/bin/cp:/bin/cp \
		.build/out/bin/mv:/bin/mv \
		.build/out/bin/cmp:/bin/cmp \
		.build/out/bin/touch:/bin/touch \
		.build/out/bin/wc:/bin/wc \
		.build/out/bin/head:/bin/head \
		.build/out/bin/grep:/bin/grep \
		.build/out/bin/sed:/bin/sed \
		.build/out/bin/ps:/bin/ps \
		.build/out/bin/kill:/bin/kill \
		.build/out/bin/sleep:/bin/sleep \
		.build/out/bin/as:/bin/as \
		.build/out/bin/ld:/bin/ld \
		.build/out/bin/cc:/bin/cc \
		.build/out/bin/ar:/bin/ar \
		.build/out/bin/buildcc:/bin/buildcc \
		.build/out/bin/buildlibc:/bin/buildlibc \
		.build/out/bin/ed:/bin/ed \
		.build/out/lib/libc.a:/lib/libc.a \
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

disk-test.img: .bin/mkfs cmd
	$(MKFS) $@ -s 2048 \
		:dir:/dev \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		testdata/hello.txt:/hello \
		testdata/large.txt:/large \
		.build/out/bin/true:/true \
		.build/out/bin/false:/false

run: clean disk.img kernel/kernel.bin
	$(QEMU_DISK) -kernel kernel/kernel.bin -append "init=/init"

test: clean disk-test.img kernel/kernel-test.bin initramfs-test.bin
	$(QEMU_TEST) -kernel kernel/kernel-test.bin -initrd initramfs-test.bin -append "init=initramfs:tests"

clean:
	rm -rf .bin/ .build/
	$(MAKE) -C kernel clean
	rm -f disk.img disk-test.img initramfs-test.bin

tidy:
	clang-format -i kernel/*.c kernel/*.h kernel/tests/*.c kernel/tests/*.h
	clang-format -i libc/*.c libc/include/*.h
	clang-format -i lib/*.h
	clang-format -i cmd/*/*.c
	clang-format -i cmd/tests/*.c
	clang-format -i cmd/cc/*.c cmd/cc/*.h
