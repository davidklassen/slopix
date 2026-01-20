.PHONY: all clean run debug test tidy

all:
	$(MAKE) -C kernel

run:
	$(MAKE) -C kernel run

debug:
	$(MAKE) -C kernel debug

test:
	$(MAKE) -C kernel test

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C cmd clean
	$(MAKE) -C kernel clean

tidy:
	clang-format -i kernel/*.c kernel/*.h kernel/tests/*.c kernel/tests/*.h
	clang-format -i libc/*.c libc/include/*.h
	clang-format -i cmd/*/*.c
