#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *srcs[] = {
    "main",
    "lexer",
    "parser",
    "encode",
    "symtab",
    "section",
    "strtab",
    "reloc",
    "elf_write",
    "literal",
    "macro",
    NULL,
};

int main(void) {
	const char *prefix = get_bin_prefix();

	mkdir_p(".build/obj");
	mkdir_p(prefix);

	for (int i = 0; srcs[i]; i++) {
		char src[64];
		snprintf(src, sizeof(src), "%s.c", srcs[i]);
		if (compile(src) != 0) return 1;
	}

	char out[256];
	snprintf(out, sizeof(out), "%s/as", prefix);
	return link_objs(out, srcs);
}
