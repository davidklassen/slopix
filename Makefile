ROOT = $(CURDIR)

BUILD = $(ROOT)/.bin/build
MKFS = $(ROOT)/.bin/mkfs
BUILD_ENV = TZ=UTC BUILD=$(BUILD) CC=$(ROOT)/.bin/cc AS=$(ROOT)/.bin/as LD=$(ROOT)/.bin/ld AR=$(ROOT)/.bin/ar \
	INCLUDE_PATH=$(ROOT)/libc/include LIB_PATH=$(ROOT)/.build/out/lib \
	BUILD_INCLUDE=$(ROOT)/lib

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_PFLASH = -drive if=pflash,format=raw,file=.build/out/bootloader.bin,readonly=on
QEMU_DISK = $(QEMU_BASE) $(QEMU_PFLASH) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) $(QEMU_PFLASH) -drive file=disk-test.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0

.PHONY: all build build-test clean run test tidy

all: build

.bin/build: cmd/build/main.c lib/build.h | .bin
	cc -I lib -std=c11 -g -Wall -Wextra -Werror -O0 -o $@ cmd/build/main.c

.bin/cc: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/cc

.bin/as: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/as

.bin/ld: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/ld

.bin/ar: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/ar

.bin/mkfs: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/mkfs

.bin/mkramfs: .bin/build | .bin
	LD=cc $(BUILD) --prefix=.bin cmd/mkramfs

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
		:dir:/boot \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		.build/out/boot/kernel.bin:/boot/kernel.bin \
		build.c:/src/build.c \
		-m .build/out:/ \
		-m boot:src/boot \
		-m cmd:src/cmd \
		-m kernel:src/kernel \
		-m lib:src/lib \
		-m libc:src/libc

disk-test.img: .bin/mkfs build-test
	$(MKFS) $@ -s 8192 \
		:dir:/dev \
		:dir:/bin \
		:dir:/boot \
		:cdev:/dev/console:1:0 \
		:cdev:/dev/null:2:0 \
		:bdev:/dev/disk:1:0 \
		.build/out/boot/kernel-test.bin:/boot/kernel.bin \
		testdata/hello.txt:/hello \
		testdata/large.txt:/large \
		.build/out/bin/true:/true \
		.build/out/bin/false:/false \
		.build/out/bin/tests:/bin/tests

run: clean disk.img
	$(QEMU_DISK)

test: clean disk-test.img
	$(QEMU_TEST) 2>&1 | ./scripts/format-tests.sh

clean:
	rm -rf .bin/ .build/
	rm -f disk.img disk-test.img initramfs-test.bin

tidy:
	find kernel libc lib cmd boot -name '*.c' -o -name '*.h' | xargs clang-format -i

