#define BUILD_IMPLEMENTATION
#include "build.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *asm_srcs[] = {
    "boot",
    "tables",
    "vectors",
    "switch",
    NULL,
};

static const char *c_srcs[] = {
    "kernel",
    "uart",
    "psci",
    "kprintf",
    "exception",
    "gic",
    "timer",
    "vmm",
    "pmm",
    "proc",
    "syscall",
    "elf",
    "init",
    "initramfs",
    "virtio",
    "bio",
    "fs",
    "file",
    "pipe",
    "console",
    "disk",
    "string",
    "sync",
    "dtb",
    "cmdline",
    NULL,
};

static const char *test_srcs[] = {
    "tests/test",
    "tests/test_uart",
    "tests/test_kprintf",
    "tests/test_exception",
    "tests/test_timer",
    "tests/test_vmm",
    "tests/test_tlb",
    "tests/test_pmm",
    "tests/test_virtio",
    "tests/test_bio",
    "tests/test_fs",
    "tests/test_file",
    "tests/test_console",
    "tests/test_pipe",
    "tests/test_string",
    "tests/test_sync",
    "tests/test_gic",
    "tests/test_dtb",
    "tests/test_cmdline",
    "tests/test_proc",
    NULL,
};

static int kernel_assemble(const char *src) {
	const char *as = get_env_or("AS", "as");
	char srcfile[256], ofile[256];

	snprintf(srcfile, sizeof(srcfile), "%s.S", src);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", src);

	Cmd cmd = {0};
	cmd_append(&cmd, as, srcfile, "-o", ofile, NULL);
	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

static int kernel_compile(const char *src, int test_mode) {
	const char *cc = get_env_or("CC", "cc");
	const char *as = get_env_or("AS", "as");
	char srcfile[256], sfile[256], ofile[256];

	snprintf(srcfile, sizeof(srcfile), "%s.c", src);
	snprintf(sfile, sizeof(sfile), ".build/obj/%s.s", src);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", src);

	Cmd cmd = {0};
	cmd_append(&cmd, cc, "-I.", NULL);
	if (test_mode) {
		cmd_append(&cmd, "-DRUN_TESTS", NULL);
	}
	cmd_append(&cmd, "-S", srcfile, "-o", sfile, NULL);

	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);
	if (ret != 0) {
		return ret;
	}

	cmd_append(&cmd, as, "-o", ofile, sfile, NULL);
	ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

static int kernel_link(const char *out, int include_tests) {
	const char *ld = get_env_or("LD", "ld");

	Cmd cmd = {0};
	cmd_append(&cmd, ld, "-T", "kernel", "--oformat=binary", "-o", out, NULL);

	for (int i = 0; asm_srcs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(asm_srcs[i]), NULL);
	}

	for (int i = 0; c_srcs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(c_srcs[i]), NULL);
	}

	if (include_tests) {
		for (int i = 0; test_srcs[i] != NULL; i++) {
			cmd_append(&cmd, make_objpath(test_srcs[i]), NULL);
		}
	}

	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

int main(void) {
	const char *run_tests = getenv("RUN_TESTS");
	int test_mode = (run_tests != NULL && run_tests[0] != '0');
	const char *output = test_mode ? ".build/out/boot/kernel-test.bin" : ".build/out/boot/kernel.bin";

	mkdir_p(".build/obj");
	mkdir_p(".build/obj/tests");
	mkdir_p(".build/out/boot");

	for (int i = 0; asm_srcs[i] != NULL; i++) {
		if (kernel_assemble(asm_srcs[i]) != 0) {
			log_error("failed to assemble %s.S", asm_srcs[i]);
			return 1;
		}
	}

	for (int i = 0; c_srcs[i] != NULL; i++) {
		if (kernel_compile(c_srcs[i], test_mode) != 0) {
			log_error("failed to compile %s.c", c_srcs[i]);
			return 1;
		}
	}

	if (test_mode) {
		for (int i = 0; test_srcs[i] != NULL; i++) {
			if (kernel_compile(test_srcs[i], test_mode) != 0) {
				log_error("failed to compile %s.c", test_srcs[i]);
				return 1;
			}
		}
	}

	if (kernel_link(output, test_mode) != 0) {
		log_error("failed to link %s", output);
		return 1;
	}

	log_info("built %s", output);
	return 0;
}
