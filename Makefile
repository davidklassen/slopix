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

disk.img: .bin/mkfs userspace
	$(MKFS) $@ -s 102400 -i 1024 \
		:dir:/dev \
		:dir:/bin \
		:dir:/lib \
		:dir:/tmp \
		:dir:/include \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		.build/out/bin/init:/bin/init \
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
		.build/out/bin/build:/bin/build \
		.build/out/bin/ed:/bin/ed \
		.build/out/bin/tests:/bin/tests \
		.build/out/bin/mkfs:/bin/mkfs \
		.build/out/bin/mkramfs:/bin/mkramfs \
		.build/out/lib/libc.a:/lib/libc.a \
		--sync-src . \
		--sync-include .build/out/include

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
	$(QEMU_DISK) -kernel kernel/kernel.bin -append "init=/bin/init"

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
