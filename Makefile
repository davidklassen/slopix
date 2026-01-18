CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I.
ASFLAGS = -g -mcpu=cortex-a57
LDFLAGS = -nostdlib -T linker.ld

OBJ = \
	boot.o \
	kernel.o \
	uart.o \
	psci.o \
	kprintf.o \
	vectors.o \
	exception.o \
	gic.o \
	timer.o \
	prompt.o

TEST_OBJ = \
	tests/test.o \
	tests/test_uart.o \
	tests/test_kprintf.o \
	tests/test_exception.o \
	tests/test_timer.o

KERNEL = kernel.elf

QEMU = qemu-system-aarch64 -M virt -cpu cortex-a57 -m 128M -nographic

.PHONY: all clean run test tidy

all: $(KERNEL)

$(KERNEL): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: clean $(KERNEL)
	$(QEMU) -kernel $(KERNEL)

test: CFLAGS += -DRUN_TESTS
test: clean $(OBJ) $(TEST_OBJ)
	$(LD) $(LDFLAGS) -o $(KERNEL) $(OBJ) $(TEST_OBJ)
	$(QEMU) -kernel $(KERNEL)

clean:
	rm -f $(OBJ) $(TEST_OBJ) $(KERNEL)

tidy:
	clang-format -i *.c *.h tests/*.c tests/*.h
