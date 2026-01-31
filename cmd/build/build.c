#define BUILD_IMPLEMENTATION
#include "build.h"

int main(void) {
	const char *prefix = get_bin_prefix();
	char outpath[256];
	snprintf(outpath, sizeof(outpath), "%s/build", prefix);

	const char *cc = getenv("CC");
	if (!cc || !cc[0]) {
		cc = "cc";
	}
	const char *as = getenv("AS");
	if (!as || !as[0]) {
		as = "as";
	}

	// Get include path for libc headers
	const char *include_path = getenv("INCLUDE_PATH");
	if (!include_path || !include_path[0]) {
		if (file_exists("/src/libc/include")) {
			include_path = "/src/libc/include";
		}
	}

	// Get include path for build.h
	const char *build_include = getenv("BUILD_INCLUDE");
	if (!build_include || !build_include[0]) {
		if (file_exists("/src/lib/build.h")) {
			build_include = "/src/lib";
		}
	}

	if (mkdir_p(".build/obj") != 0) {
		log_error("cannot create .build/obj");
		return 1;
	}

	// Create output directory
	char outdir[256];
	snprintf(outdir, sizeof(outdir), "%s", prefix);
	if (mkdir_p(outdir) != 0) {
		log_error("cannot create %s", outdir);
		return 1;
	}

	Cmd cmd = {0};

	// Compile - need both libc include and build.h include
	cmd_append(&cmd, cc, NULL);
	if (include_path && include_path[0]) {
		static char incflag[256];
		snprintf(incflag, sizeof(incflag), "-I%s", include_path);
		cmd_append(&cmd, incflag, NULL);
	}
	if (build_include && build_include[0]) {
		static char buildflag[256];
		snprintf(buildflag, sizeof(buildflag), "-I%s", build_include);
		cmd_append(&cmd, buildflag, NULL);
	}
	cmd_append(&cmd, "-S", "main.c", "-o", ".build/obj/main.s", NULL);
	if (cmd_run(&cmd) != 0) {
		return 1;
	}
	cmd_reset(&cmd);

	// Assemble
	cmd_append(&cmd, as, "-o", ".build/obj/main.o", ".build/obj/main.s", NULL);
	if (cmd_run(&cmd) != 0) {
		return 1;
	}
	cmd_reset(&cmd);

	// Link
	const char *objs[] = {"main", NULL};
	if (link_objs(outpath, objs) != 0) {
		return 1;
	}

	return 0;
}
