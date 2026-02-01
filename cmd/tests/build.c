#define BUILD_IMPLEMENTATION
#include "build.h"

static const char *srcs[] = {
    "tests",
    "test_syscall",
    "test_sched",
    "test_memory",
    "test_mmap",
    "test_filesys",
    "test_pipe",
    "test_libc",
    "test_devices",
    "test_codegen",
    "test_malloc",
    "test_stdio",
    "test_libgen",
    "test_dirent",
    "test_errno",
    "test_sbrk",
    "test_errors",
    NULL,
};

int main(void) {
	mkdir_p(".build/obj");
	mkdir_p(".build/out/bin");

	for (int i = 0; srcs[i]; i++) {
		char src[64];
		snprintf(src, sizeof(src), "%s.c", srcs[i]);
		if (compile(src) != 0) return 1;
	}

	return link_objs(".build/out/bin/tests", srcs);
}
