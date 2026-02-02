#define BUILD_IMPLEMENTATION
#include "build.h"

#include <stdio.h>
#include <unistd.h>

static const char *asm_srcs[] = {
    "start",
    NULL,
};

static const char *c_srcs[] = {
    "main",
    "uart",
    NULL,
};

static int boot_assemble(const char *src) {
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

static int boot_compile(const char *src) {
	const char *cc = get_env_or("CC", "cc");
	const char *as = get_env_or("AS", "as");
	char srcfile[256], sfile[256], ofile[256];

	snprintf(srcfile, sizeof(srcfile), "%s.c", src);
	snprintf(sfile, sizeof(sfile), ".build/obj/%s.s", src);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", src);

	Cmd cmd = {0};
	cmd_append(&cmd, cc, "-S", srcfile, "-o", sfile, NULL);

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

static int boot_link(const char *out) {
	const char *ld = get_env_or("LD", "ld");

	Cmd cmd = {0};
	cmd_append(&cmd, ld, "-T", "bootloader", "--oformat=binary", "-o", out, NULL);

	for (int i = 0; asm_srcs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(asm_srcs[i]), NULL);
	}

	for (int i = 0; c_srcs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(c_srcs[i]), NULL);
	}

	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

int main(void) {
	mkdir_p(".build/obj");
	mkdir_p(".build/out");

	for (int i = 0; asm_srcs[i] != NULL; i++) {
		if (boot_assemble(asm_srcs[i]) != 0) {
			log_error("failed to assemble %s.S", asm_srcs[i]);
			return 1;
		}
	}

	for (int i = 0; c_srcs[i] != NULL; i++) {
		if (boot_compile(c_srcs[i]) != 0) {
			log_error("failed to compile %s.c", c_srcs[i]);
			return 1;
		}
	}

	if (boot_link(".build/out/bootloader.bin") != 0) {
		log_error("failed to link bootloader.bin");
		return 1;
	}

	if (truncate(".build/out/bootloader.bin", 64 * 1024 * 1024) != 0) {
		log_error("failed to pad bootloader.bin");
		return 1;
	}

	log_info("built bootloader.bin");
	return 0;
}
