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
	mkdir_p("build/lib");

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

	return archive_objs("build/lib/libc.a", all_objs);
}
