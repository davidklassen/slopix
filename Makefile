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

BUILD_ENV = BUILD=$(BUILD) CC=$(CC) AS=$(AS) LD=$(LD) AR=$(ROOT)/.bin/ar \
	INCLUDE_PATH=$(LIBC_INCLUDE) LIB_PATH=$(ROOT)/.build/out/lib \
	BUILD_INCLUDE=$(ROOT)/lib

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_DISK = $(QEMU_BASE) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) -drive file=disk-test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

.PHONY: all build build-test clean run test tidy

all: build

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

build: .bin/cc .bin/as .bin/ld .bin/ar
	$(BUILD_ENV) $(BUILD)

build-test: .bin/cc .bin/as .bin/ld .bin/ar
	$(BUILD_ENV) RUN_TESTS=1 $(BUILD)

disk.img: .bin/mkfs build
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

disk-test.img: .bin/mkfs build-test
	$(MKFS) $@ -s 4096 \
		:dir:/dev \
		:dir:/bin \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		testdata/hello.txt:/hello \
		testdata/large.txt:/large \
		.build/out/bin/true:/true \
		.build/out/bin/false:/false \
		.build/out/bin/tests:/bin/tests

run: clean disk.img
	$(QEMU_DISK) -kernel .build/out/boot/kernel.bin -append "init=/bin/init"

test: clean disk-test.img
	$(QEMU_TEST) -kernel .build/out/boot/kernel-test.bin -append "init=/bin/tests"

clean:
	rm -rf .bin/ .build/
	rm -f disk.img disk-test.img initramfs-test.bin

tidy:
	find kernel libc lib cmd boot -name '*.c' -o -name '*.h' | xargs clang-format -i

test-bootloader: clean disk.img
	$(QEMU_BASE) \
		-drive if=pflash,format=raw,file=.build/out/bootloader.bin,readonly=on \
		-drive file=disk.img,if=none,format=raw,id=hd0 \
		-device virtio-blk-device,drive=hd0
