HOSTCC = cc
CFLAGS = -Wall -Wextra -Werror -O2

mkfs: mkfs.c
	$(HOSTCC) $(CFLAGS) -o $@ $<

clean:
	rm -f mkfs

.PHONY: clean
