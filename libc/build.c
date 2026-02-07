#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *c_srcs[] = {
    "ctype",
    "dirent",
    "errno",
    "libgen",
    "malloc",
    "stdio",
    "stdio_file",
    "stdlib",
    "string",
    "test",
    "time",
    NULL,
};

static const char *asm_srcs[] = {
    "crt0",
    "syscall",
    NULL,
};

int main(void) {
	mkdir_p(".build/obj");
	mkdir_p(".build/out/lib");

	for (int i = 0; c_srcs[i]; i++) {
		char src[64];
		snprintf(src, sizeof(src), "%s.c", c_srcs[i]);
		if (compile(src) != 0) return 1;
	}

	for (int i = 0; asm_srcs[i]; i++) {
		char src[64];
		snprintf(src, sizeof(src), "%s.S", asm_srcs[i]);
		if (assemble(src) != 0) return 1;
	}

	int c_count = 0, asm_count = 0;
	for (int i = 0; c_srcs[i]; i++)
		c_count++;
	for (int i = 0; asm_srcs[i]; i++)
		asm_count++;

	const char *all_objs[c_count + asm_count + 1];
	int n = 0;
	for (int i = 0; i < c_count; i++)
		all_objs[n++] = c_srcs[i];
	for (int i = 0; i < asm_count; i++)
		all_objs[n++] = asm_srcs[i];
	all_objs[n] = NULL;

	if (archive_objs(".build/out/lib/libc.a", all_objs) != 0) return 1;

	mkdir_p(".build/out/include");
	if (copy_dir("include", ".build/out/include") < 0) {
		log_error("failed to install headers");
		return 1;
	}

	return 0;
}
