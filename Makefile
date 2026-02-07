ROOT = $(CURDIR)

HOSTCC ?= cc
HOSTAS ?= as
CROSSCC = $(ROOT)/.tools/cc
CROSSAS = $(ROOT)/.tools/as
CROSSLD = $(ROOT)/.tools/ld
CROSSAR = $(ROOT)/.tools/ar

BUILD = $(ROOT)/.tools/build
MKFS = $(ROOT)/.tools/mkfs
CFLAGS ?=
LDFLAGS ?=
LDLIBS ?=
HOST_BUILD_ENV = CC=$(HOSTCC) AS=$(HOSTAS) LD=$(HOSTCC) CFLAGS= LDFLAGS= LDLIBS=
CROSS_BUILD_ENV = TZ=UTC BUILD=$(BUILD) CC=$(CROSSCC) AS=$(CROSSAS) LD=$(CROSSLD) AR=$(CROSSAR) \
	CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" LDLIBS="$(LDLIBS)" \
	INCLUDE_PATH=$(ROOT)/libc/include LIB_PATH=$(ROOT)/.build/out/lib \
	BUILD_INCLUDE=$(ROOT)/lib

QEMU_BASE = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic
QEMU_PFLASH = -drive if=pflash,format=raw,file=.build/out/bootloader.bin,readonly=on
QEMU_DISK = $(QEMU_BASE) $(QEMU_PFLASH) -drive file=disk.img,if=none,format=raw,id=hd0 -device virtio-blk-device,drive=hd0
QEMU_TEST = $(QEMU_BASE) $(QEMU_PFLASH) -drive file=disk-test.img,if=none,format=raw,snapshot=on,id=hd0 -device virtio-blk-device,drive=hd0

SOURCES := $(shell find kernel libc boot cmd lib -name '*.c' -o -name '*.h' -o -name '*.S')
TESTDATA := testdata/hello.txt testdata/large.txt
TOOLS := .tools/cc .tools/as .tools/ld .tools/ar .tools/mkfs

BUILD_SOURCES := cmd/build/main.c lib/build.h
CC_SOURCES := $(shell find cmd/cc -name '*.c' -o -name '*.h')
AS_SOURCES := $(shell find cmd/as -name '*.c' -o -name '*.h')
LD_SOURCES := $(shell find cmd/ld -name '*.c' -o -name '*.h')
AR_SOURCES := $(shell find cmd/ar -name '*.c' -o -name '*.h')
MKFS_SOURCES := $(shell find cmd/mkfs -name '*.c' -o -name '*.h')
MKRAMFS_SOURCES := $(shell find cmd/mkramfs -name '*.c' -o -name '*.h')

.PHONY: all build build-test clean run test tidy

all: build

.tools/build: $(BUILD_SOURCES) | .tools
	$(HOSTCC) -I lib -std=c11 -g -Wall -Wextra -Werror -O0 -o $@ cmd/build/main.c

.tools/cc: $(CC_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/cc

.tools/as: $(AS_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/as

.tools/ld: $(LD_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/ld

.tools/ar: $(AR_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/ar

.tools/mkfs: $(MKFS_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/mkfs

.tools/mkramfs: $(MKRAMFS_SOURCES) lib/build.h .tools/build | .tools
	$(HOST_BUILD_ENV) $(BUILD) --prefix=.tools cmd/mkramfs

.tools:
	mkdir -p $@

build: $(TOOLS)
	$(CROSS_BUILD_ENV) $(BUILD)

build-test: $(TOOLS)
	$(CROSS_BUILD_ENV) RUN_TESTS=1 $(BUILD)

disk.img: $(SOURCES) $(TOOLS)
	$(CROSS_BUILD_ENV) $(BUILD)
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

disk-test.img: $(SOURCES) $(TESTDATA) $(TOOLS)
	$(CROSS_BUILD_ENV) RUN_TESTS=1 $(BUILD)
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

run: disk.img
	$(QEMU_DISK)

test: disk-test.img
	$(QEMU_TEST) 2>&1 | ./scripts/format-tests.sh

clean:
	rm -rf .tools/ .build/
	rm -f disk.img disk-test.img initramfs-test.bin

tidy:
	find kernel libc lib cmd boot -name '*.c' -o -name '*.h' | xargs clang-format -i
