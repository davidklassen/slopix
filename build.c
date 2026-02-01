#define BUILD_IMPLEMENTATION
#include "build.h"

int main(void) {
	if (build_subdir("libc") != 0) {
		return 1;
	}

	mkdir_p(".build/out/include");
	if (copy_file("lib/build.h", ".build/out/include/build.h") < 0) {
		log_error("failed to install build.h");
		return 1;
	}

	if (build_subdir("cmd") != 0) {
		return 1;
	}
	return 0;
}
