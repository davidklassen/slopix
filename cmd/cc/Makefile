CFLAGS = -I$(CC_INCLUDE) -I$(LIBC_INCLUDE)

SRCS = main.c tokenize.c preprocess.c parse.c type.c codegen.c \
       unicode.c strings.c hashmap.c
OBJS = $(SRCS:.c=.o)

cc.elf: $(OBJS)
	$(LD) -o $@ $(OBJS) $(LIBC)

%.o: %.c chibicc.h
	$(CC) $(CFLAGS) -S -o $*.s $<
	$(AS) -o $@ $*.s

clean:
	rm -f *.o $(SRCS:.c=.s) cc.elf

.PHONY: clean
