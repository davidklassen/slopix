# Userspace tests: compiled by chibicc, runs on slopix
# CC, AS, LD, LIBC, LIBC_INCLUDE must be passed from parent

CFLAGS = -I$(LIBC_INCLUDE)

OBJ = tests.o test_syscall.o test_sched.o test_memory.o test_mmap.o \
      test_filesys.o test_pipe.o test_libc.o test_devices.o test_codegen.o test_malloc.o test_stdio.o test_libgen.o test_dirent.o

all: tests.elf

tests.elf: $(OBJ)
	$(LD) -o $@ $(OBJ) $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o *.s tests.elf

.PHONY: all clean
