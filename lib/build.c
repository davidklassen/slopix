#define BUILD_IMPLEMENTATION
#include "build.h"

int main(void) {
	mkdir_p("../.build/out/include");
	if (copy_file("build.h", "../.build/out/include/build.h") < 0) {
		log_error("failed to install build.h");
		return 1;
	}
	return 0;
}
