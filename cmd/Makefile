CROSS = aarch64-elf-
CC = $(CROSS)gcc
LD = $(CROSS)ld
HOSTCC = cc

CFLAGS = -Wall -Wextra -Werror -O2 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=cortex-a57 -I../libc/include
LDFLAGS = -nostdlib -T link.ld

LIBC = ../libc/libc.a

APPS = shell cursor_blink echo ticker shutdown true false
TEST_APPS = tests true false

PROGS = $(addsuffix .elf,$(APPS))
TEST_PROGS = $(addsuffix .elf,$(TEST_APPS))

all: initramfs.bin

$(LIBC):
	$(MAKE) -C ../libc

mkramfs:
	$(MAKE) -C ../tools mkramfs
	cp ../tools/mkramfs $@

shell.elf: shell/shell.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

cursor_blink.elf: cursor_blink/cursor_blink.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

echo.elf: echo/echo.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

ticker.elf: ticker/ticker.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

shutdown.elf: shutdown/shutdown.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

true.elf: true/true.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

false.elf: false/false.o $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

TEST_OBJ = \
	tests/tests.o \
	tests/test_syscall.o \
	tests/test_sched.o \
	tests/test_memory.o \
	tests/test_filesys.o \
	tests/test_pipe.o

tests.elf: $(TEST_OBJ) $(LIBC)
	$(LD) $(LDFLAGS) -o $@ $(TEST_OBJ) $(LIBC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

initramfs.bin: mkramfs $(PROGS)
	./mkramfs $@ $(PROGS)

initramfs-test.bin: mkramfs $(TEST_PROGS)
	./mkramfs $@ $(TEST_PROGS)

clean:
	rm -f */*.o *.elf mkramfs initramfs.bin initramfs-test.bin

.PHONY: all clean
