CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I$(LIBC_INCLUDE)
LDFLAGS = -nostdlib -T $(LINK_LD)

OBJ = tests.o test_syscall.o test_sched.o test_memory.o test_mmap.o \
      test_filesys.o test_pipe.o test_libc.o test_devices.o

all: tests.elf

tests.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o tests.elf

.PHONY: all clean
