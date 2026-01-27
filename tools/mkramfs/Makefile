HOSTCC = cc
CFLAGS = -Wall -Wextra -Werror -O2

mkramfs: mkramfs.c
	$(HOSTCC) $(CFLAGS) -o $@ $<

clean:
	rm -f mkramfs

.PHONY: clean
