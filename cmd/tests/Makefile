AS = aarch64-elf-as
LD = aarch64-elf-ld

CFLAGS = -I$(LIBC_INCLUDE) -I$(CC_INCLUDE)
LDFLAGS = -nostdlib -T $(LINK_LD)

OBJ = tests.o test_syscall.o test_sched.o test_memory.o test_mmap.o \
      test_filesys.o test_pipe.o test_libc.o test_devices.o test_codegen.o test_malloc.o test_stdio.o

all: tests.elf

tests.elf: $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $(OBJ) $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s tests.elf

.PHONY: all clean
