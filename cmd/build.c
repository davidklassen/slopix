#define BUILD_IMPLEMENTATION
#include "build.h"

int main(void) {
	if (build_subdir("cc") != 0) return 1;
	if (build_subdir("as") != 0) return 1;
	if (build_subdir("ld") != 0) return 1;
	if (build_subdir("tests") != 0) return 1;

	if (build_subdir("ar") != 0) return 1;
	if (build_subdir("build") != 0) return 1;
	if (build_subdir("cat") != 0) return 1;
	if (build_subdir("cmp") != 0) return 1;
	if (build_subdir("cp") != 0) return 1;
	if (build_subdir("cursor_blink") != 0) return 1;
	if (build_subdir("echo") != 0) return 1;
	if (build_subdir("ed") != 0) return 1;
	if (build_subdir("false") != 0) return 1;
	if (build_subdir("grep") != 0) return 1;
	if (build_subdir("head") != 0) return 1;
	if (build_subdir("init") != 0) return 1;
	if (build_subdir("kill") != 0) return 1;
	if (build_subdir("ls") != 0) return 1;
	if (build_subdir("mkdir") != 0) return 1;
	if (build_subdir("mkfs") != 0) return 1;
	if (build_subdir("mkramfs") != 0) return 1;
	if (build_subdir("mv") != 0) return 1;
	if (build_subdir("ps") != 0) return 1;
	if (build_subdir("rm") != 0) return 1;
	if (build_subdir("sed") != 0) return 1;
	if (build_subdir("shell") != 0) return 1;
	if (build_subdir("shutdown") != 0) return 1;
	if (build_subdir("reboot") != 0) return 1;
	if (build_subdir("sleep") != 0) return 1;
	if (build_subdir("ticker") != 0) return 1;
	if (build_subdir("touch") != 0) return 1;
	if (build_subdir("true") != 0) return 1;
	if (build_subdir("wc") != 0) return 1;

	return 0;
}
