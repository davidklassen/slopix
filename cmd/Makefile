CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
HOSTCC = cc

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I.
ASFLAGS = -g -mcpu=cortex-a57
LDFLAGS = -nostdlib -T link.ld

COMMON_OBJ = crt0.o syscall.o libc.o

PROGS = init.elf hello.elf

all: initramfs.bin

mkramfs: ../tools/mkramfs.c
	$(HOSTCC) -Wall -o $@ $<

$(PROGS): %.elf: %.o $(COMMON_OBJ)
	$(LD) $(LDFLAGS) -o $@ $(COMMON_OBJ) $<

initramfs.bin: mkramfs $(PROGS)
	./mkramfs $@ $(PROGS)

%.o: %.S
	$(CC) $(ASFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o *.elf mkramfs initramfs.bin

.PHONY: all clean
