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
		if (compile(src) != 0) {
			return 1;
		}
	}

	for (int i = 0; asm_srcs[i]; i++) {
		char src[64];
		snprintf(src, sizeof(src), "%s.S", asm_srcs[i]);
		if (assemble(src) != 0) {
			return 1;
		}
	}

	static const char *all_objs[] = {
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
	    "crt0",
	    "syscall",
	    NULL,
	};

	if (archive_objs(".build/out/lib/libc.a", all_objs) != 0) {
		return 1;
	}

	mkdir_p(".build/out/include");
	if (copy_dir("include", ".build/out/include") < 0) {
		log_error("failed to install headers");
		return 1;
	}

	return 0;
}
