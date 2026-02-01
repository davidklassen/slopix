#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *srcs[] = {
    "main",
    "elf_read",
    "symbol",
    "section",
    "reloc",
    "output",
    "archive",
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
	snprintf(out, sizeof(out), "%s/ld", prefix);
	return link_objs(out, srcs);
}
