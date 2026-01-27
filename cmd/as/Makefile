CC = cc
CFLAGS = -Wall -Wextra -g -O0

SRC = main.c lexer.c symtab.c parser.c encode.c section.c strtab.c reloc.c elf_write.c
OBJ = $(SRC:.c=.o)

as: $(OBJ)
	$(CC) -o $@ $(OBJ)

%.o: %.c as.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) as

.PHONY: clean
