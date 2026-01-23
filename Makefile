.PHONY: all clean run debug test tidy mkfs

all:
	$(MAKE) -C kernel

run:
	$(MAKE) -C kernel run

debug:
	$(MAKE) -C kernel debug

test:
	$(MAKE) -C kernel test

mkfs:
	$(MAKE) -C tools mkfs

disk.img: mkfs
	./tools/mkfs $@

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C cmd clean
	$(MAKE) -C kernel clean
	$(MAKE) -C tools clean

tidy:
	clang-format -i kernel/*.c kernel/*.h kernel/tests/*.c kernel/tests/*.h
	clang-format -i libc/*.c libc/include/*.h
	clang-format -i cmd/*/*.c
	clang-format -i tools/*.c
