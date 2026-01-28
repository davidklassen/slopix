ROOT = $(dir $(lastword $(MAKEFILE_LIST)))..
AS ?= $(ROOT)/tools/as/as
LD ?= $(ROOT)/tools/ld/ld

CFLAGS = -I$(CC_INCLUDE) -I$(LIBC_INCLUDE)

PROGS = init.elf shell.elf cursor_blink.elf echo.elf ticker.elf shutdown.elf \
	true.elf false.elf cat.elf ls.elf mkdir.elf rm.elf cp.elf mv.elf cmp.elf \
	touch.elf wc.elf head.elf grep.elf sed.elf ps.elf kill.elf sleep.elf tests.elf \
	as.elf ld.elf cc.elf buildcc.elf

all: $(PROGS)

init.elf: init/init.o
	$(LD) -o $@ $< $(LIBC)

shell.elf: shell/shell.o
	$(LD) -o $@ $< $(LIBC)

cursor_blink.elf: cursor_blink/cursor_blink.o
	$(LD) -o $@ $< $(LIBC)

echo.elf: echo/echo.o
	$(LD) -o $@ $< $(LIBC)

ticker.elf: ticker/ticker.o
	$(LD) -o $@ $< $(LIBC)

shutdown.elf: shutdown/shutdown.o
	$(LD) -o $@ $< $(LIBC)

true.elf: true/true.o
	$(LD) -o $@ $< $(LIBC)

false.elf: false/false.o
	$(LD) -o $@ $< $(LIBC)

cat.elf: cat/cat.o
	$(LD) -o $@ $< $(LIBC)

ls.elf: ls/ls.o
	$(LD) -o $@ $< $(LIBC)

mkdir.elf: mkdir/mkdir.o
	$(LD) -o $@ $< $(LIBC)

rm.elf: rm/rm.o
	$(LD) -o $@ $< $(LIBC)

cp.elf: cp/cp.o
	$(LD) -o $@ $< $(LIBC)

mv.elf: mv/mv.o
	$(LD) -o $@ $< $(LIBC)

cmp.elf: cmp/cmp.o
	$(LD) -o $@ $< $(LIBC)

touch.elf: touch/touch.o
	$(LD) -o $@ $< $(LIBC)

wc.elf: wc/wc.o
	$(LD) -o $@ $< $(LIBC)

head.elf: head/head.o
	$(LD) -o $@ $< $(LIBC)

grep.elf: grep/grep.o
	$(LD) -o $@ $< $(LIBC)

sed.elf: sed/sed.o
	$(LD) -o $@ $< $(LIBC)

ps.elf: ps/ps.o
	$(LD) -o $@ $< $(LIBC)

kill.elf: kill/kill.o
	$(LD) -o $@ $< $(LIBC)

sleep.elf: sleep/sleep.o
	$(LD) -o $@ $< $(LIBC)

buildcc.elf: buildcc/buildcc.o
	$(LD) -o $@ $< $(LIBC)

tests.elf:
	$(MAKE) -C tests CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)
	cp tests/tests.elf $@

as.elf:
	$(MAKE) -C as CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)
	cp as/as.elf $@

ld.elf:
	$(MAKE) -C ld CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)
	cp ld/ld.elf $@

cc.elf:
	$(MAKE) -C cc CC=$(CC) AS=$(AS) LD=$(LD) LIBC=$(LIBC) LIBC_INCLUDE=$(LIBC_INCLUDE) CC_INCLUDE=$(CC_INCLUDE)
	cp cc/cc.elf $@

%.o: %.c
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f */*.o */*.s *.elf
	$(MAKE) -C tests clean
	$(MAKE) -C as clean
	$(MAKE) -C ld clean
	$(MAKE) -C cc clean

.PHONY: all clean
