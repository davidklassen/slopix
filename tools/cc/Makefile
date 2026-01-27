HOSTCC = cc
CFLAGS = -std=c11 -g -fno-common -Wall -Wno-switch

SRCS = main.c tokenize.c preprocess.c parse.c type.c codegen.c \
       unicode.c strings.c hashmap.c
OBJS = $(SRCS:.c=.o)

chibicc: $(OBJS)
	$(HOSTCC) $(CFLAGS) -o $@ $^

$(OBJS): chibicc.h

%.o: %.c
	$(HOSTCC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f chibicc $(OBJS)

.PHONY: clean
