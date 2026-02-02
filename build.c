#define BUILD_IMPLEMENTATION
#include "build.h"

int main(void) {
	if (build_subdir("lib") != 0) return 1;
	if (build_subdir("libc") != 0) return 1;
	if (build_subdir("cmd") != 0) return 1;
	if (build_subdir("kernel") != 0) return 1;
	return 0;
}
