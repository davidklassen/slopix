CROSS = aarch64-elf-
CC = ../.bin/cc
LD = $(CROSS)ld
OBJCOPY = $(CROSS)objcopy
AS = ../.bin/as
GNU_AS = $(CROSS)as

CFLAGS = -I.
ASFLAGS = -g -mcpu=cortex-a57
LDFLAGS = -nostdlib -T linker.ld

OBJ = \
	boot.o \
	tables.o \
	kernel.o \
	uart.o \
	psci.o \
	kprintf.o \
	vectors.o \
	exception.o \
	gic.o \
	timer.o \
	vmm.o \
	pmm.o \
	proc.o \
	switch.o \
	syscall.o \
	elf.o \
	init.o \
	initramfs.o \
	virtio.o \
	bio.o \
	fs.o \
	file.o \
	pipe.o \
	console.o \
	disk.o \
	string.o \
	sync.o \
	dtb.o \
	cmdline.o

TEST_OBJ = \
	tests/test.o \
	tests/test_uart.o \
	tests/test_kprintf.o \
	tests/test_exception.o \
	tests/test_timer.o \
	tests/test_vmm.o \
	tests/test_tlb.o \
	tests/test_pmm.o \
	tests/test_initramfs.o \
	tests/test_virtio.o \
	tests/test_bio.o \
	tests/test_fs.o \
	tests/test_file.o \
	tests/test_console.o \
	tests/test_pipe.o \
	tests/test_string.o \
	tests/test_sync.o \
	tests/test_gic.o \
	tests/test_dtb.o \
	tests/test_cmdline.o

.PHONY: all clean

all: kernel.bin

kernel.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

kernel-test.elf: CFLAGS += -DRUN_TESTS
kernel-test.elf: $(OBJ) $(TEST_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(TEST_OBJ)

kernel-test.bin: kernel-test.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.S $(AS)
	$(AS) $< -o $@

# Two-step compilation: C -> .s -> .o (custom cc produces assembly, GNU as assembles)
%.o: %.c $(CC)
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(GNU_AS) $(ASFLAGS) -o $@ $*.s

clean:
	rm -f *.o *.s *.elf *.bin tests/*.o tests/*.s
