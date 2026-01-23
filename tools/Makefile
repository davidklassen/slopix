HOSTCC = cc
CFLAGS = -Wall -Wextra -Werror -O2

all: mkfs mkramfs

mkfs: mkfs.c
	$(HOSTCC) $(CFLAGS) -o $@ $<

mkramfs: mkramfs.c
	$(HOSTCC) $(CFLAGS) -o $@ $<

clean:
	rm -f mkfs mkramfs

.PHONY: all clean
