ROOT = $(dir $(lastword $(MAKEFILE_LIST)))..
AS ?= $(ROOT)/tools/as/as
LD ?= aarch64-elf-ld

CFLAGS = -I$(CC_INCLUDE) -I$(LIBC_INCLUDE)
LDFLAGS = -nostdlib -T link.ld

PROGS = init.elf shell.elf cursor_blink.elf echo.elf ticker.elf shutdown.elf \
	true.elf false.elf cat.elf ls.elf mkdir.elf rm.elf cp.elf mv.elf \
	touch.elf wc.elf head.elf grep.elf ps.elf kill.elf sleep.elf tests.elf \
	as.elf

all: $(PROGS)

init.elf: init/init.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

shell.elf: shell/shell.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

cursor_blink.elf: cursor_blink/cursor_blink.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

echo.elf: echo/echo.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

ticker.elf: ticker/ticker.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

shutdown.elf: shutdown/shutdown.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

true.elf: true/true.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

false.elf: false/false.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

cat.elf: cat/cat.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

ls.elf: ls/ls.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

mkdir.elf: mkdir/mkdir.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

rm.elf: rm/rm.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

cp.elf: cp/cp.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

mv.elf: mv/mv.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

touch.elf: touch/touch.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

wc.elf: wc/wc.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

head.elf: head/head.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

grep.elf: grep/grep.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

ps.elf: ps/ps.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

kill.elf: kill/kill.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

sleep.elf: sleep/sleep.o
	$(LD) $(LDFLAGS) -o $@ $< $(LIBC)

tests.elf:
	$(MAKE) -C tests CC=$(CC) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) LINK_LD=$(CURDIR)/link.ld CC_INCLUDE=$(CC_INCLUDE)
	cp tests/tests.elf $@

as.elf:
	$(MAKE) -C as CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)
	cp as/as.elf $@

%.o: %.c
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f */*.o */*.s *.elf
	$(MAKE) -C tests clean
	$(MAKE) -C as clean

.PHONY: all clean
