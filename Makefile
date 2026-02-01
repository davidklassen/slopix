ROOT = $(CURDIR)

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

.PHONY: all clean run test tidy userspace

all: kernel/kernel.bin

.bin/build: cmd/build/main.c lib/build.h | .bin
	$(HOSTCC) -I lib $(HOST_CFLAGS) -o $@ cmd/build/main.c

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

.bin:
	mkdir -p $@

userspace: .bin/cc .bin/as .bin/ld .bin/ar
	BUILD=$(BUILD) CC=$(CC) AS=$(AS) LD=$(LD) AR=$(ROOT)/.bin/ar \
	INCLUDE_PATH=$(LIBC_INCLUDE) LIB_PATH=$(ROOT)/.build/out/lib \
	BUILD_INCLUDE=$(ROOT)/lib \
	$(BUILD)

initramfs-test.bin: .bin/mkramfs userspace
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
		:dir:/tmp \
		:dir:/src \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		build.c:/src/build.c \
		-m .build/out:/ \
		-m cmd:src/cmd \
		-m kernel:src/kernel \
		-m lib:src/lib \
		-m libc:src/libc

disk-test.img: .bin/mkfs userspace
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
	find kernel libc lib cmd -name '*.c' -o -name '*.h' | xargs clang-format -i
