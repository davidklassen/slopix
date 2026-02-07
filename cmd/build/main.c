// build - slopix build tool
//
// Usage: build [--prefix=PATH] [clean] [DIR]
//
// Finds build.c in the target directory, compiles it with the system
// compiler, and runs it. For directories without build.c, falls back
// to compiling a single .c file directly.
//
// Environment variables (used by compiled build.c):
//   BUILD_PREFIX   Output directory for binaries (set by --prefix)
//   BUILD_INCLUDE  Path to lib/build.h
//   CC, AS, LD, AR Toolchain programs
//   INCLUDE_PATH   Header search path for compiled programs
//   LIB_PATH       Library path (libc.a location)

#define BUILD_IMPLEMENTATION
#include "build.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void usage(void) {
	fprintf(stderr, "Usage: build [--prefix=PATH] [clean] [DIR]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  --prefix=PATH  Output directory for binaries\n");
	fprintf(stderr, "  clean          Remove .build/ directory\n");
	fprintf(stderr, "  DIR            Directory to build (default: current)\n");
	exit(1);
}

static int is_dir(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int run_cmd(char *const argv[]) {
	fprintf(stderr, "[build]");
	for (int i = 0; argv[i]; i++) {
		fprintf(stderr, " %s", argv[i]);
	}
	fprintf(stderr, "\n");

	int pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}
	if (pid == 0) {
		execvp(argv[0], argv);
		perror(argv[0]);
		exit(127);
	}

	int status;
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		return WEXITSTATUS(status);
	}
	return 1;
}

static char *find_single_c_file(void) {
	DIR *d = opendir(".");
	if (d == NULL) {
		return NULL;
	}

	char *found = NULL;
	int count = 0;
	struct dirent *ent;

	while ((ent = readdir(d)) != NULL) {
		size_t len = strlen(ent->d_name);
		if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0) {
			if (strcmp(ent->d_name, "build.c") == 0) {
				continue;
			}
			count++;
			if (count == 1) {
				found = strdup(ent->d_name);
			} else {
				free(found);
				found = NULL;
			}
		}
	}
	closedir(d);

	if (count != 1) {
		free(found);
		return NULL;
	}
	return found;
}

static char *get_dirname(void) {
	char cwd[512];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		return NULL;
	}
	const char *base = basename_c(cwd);
	return strdup(base);
}

static int build_with_buildc(const char *prefix, const char *build_include) {
	if (mkdir_p(".build") < 0) {
		fprintf(stderr, "[build] ERROR: cannot create .build/\n");
		return 1;
	}

	// Always use system "cc" here, not $CC which may be a cross-compiler
	// Skip -I flag when using default /include (cc already includes it)
	int ret;
	if (strcmp(build_include, "/include") == 0) {
		char *compile_argv[] = {"cc", "build.c", "-o", ".build/build", NULL};
		ret = run_cmd(compile_argv);
	} else {
		char incflag[256];
		snprintf(incflag, sizeof(incflag), "-I%s", build_include);
		char *compile_argv[] = {"cc", incflag, "build.c", "-o", ".build/build", NULL};
		ret = run_cmd(compile_argv);
	}
	if (ret != 0) {
		fprintf(stderr, "[build] ERROR: failed to compile build.c\n");
		return ret;
	}

	setenv("BUILD_PREFIX", prefix, 1);

	char *exec_argv[] = {".build/build", NULL};
	ret = run_cmd(exec_argv);
	return ret;
}

static int build_fallback(const char *prefix) {
	char *src = find_single_c_file();
	if (src == NULL) {
		fprintf(stderr,
			"[build] ERROR: no build.c and not exactly one .c file\n");
		return 1;
	}

	char *dirname = get_dirname();
	if (dirname == NULL) {
		fprintf(stderr, "[build] ERROR: cannot determine directory name\n");
		free(src);
		return 1;
	}

	if (mkdir_p(prefix) < 0) {
		fprintf(stderr, "[build] ERROR: cannot create %s/\n", prefix);
		free(src);
		free(dirname);
		return 1;
	}

	// Use build.h's compile() which handles INCLUDE_PATH detection
	int ret = compile(src);
	if (ret != 0) {
		free(src);
		free(dirname);
		return ret;
	}

	// Build output path and link
	char outfile[512];
	snprintf(outfile, sizeof(outfile), "%s/%s", prefix, dirname);

	// Extract base name without .c extension
	size_t srclen = strlen(src);
	char base[256];
	snprintf(base, sizeof(base), "%.*s", (int)(srclen - 2), src);

	const char *objs[] = {base, NULL};
	ret = link_objs(outfile, objs);

	free(src);
	free(dirname);
	return ret;
}

int main(int argc, char **argv) {
	const char *prefix_arg = ".build/out/bin";
	const char *dir = NULL;
	int do_clean = 0;

	for (int i = 1; i < argc; i++) {
		if (strncmp(argv[i], "--prefix=", 9) == 0) {
			prefix_arg = argv[i] + 9;
		} else if (strcmp(argv[i], "clean") == 0) {
			do_clean = 1;
		} else if (strcmp(argv[i], "--help") == 0 ||
			   strcmp(argv[i], "-h") == 0) {
			usage();
		} else if (argv[i][0] == '-') {
			fprintf(stderr, "[build] ERROR: unknown option: %s\n", argv[i]);
			usage();
		} else {
			if (dir != NULL) {
				fprintf(stderr, "[build] ERROR: multiple directories\n");
				usage();
			}
			dir = argv[i];
		}
	}

	char cwd[512];
	if (getcwd(cwd, sizeof(cwd)) == NULL) {
		perror("getcwd");
		return 1;
	}

	// Convert relative paths to absolute before chdir
	const char *build_include_env = getenv("BUILD_INCLUDE");
	char build_include[512];
	if (build_include_env != NULL && build_include_env[0] != '\0') {
		if (build_include_env[0] != '/') {
			if (pathfmt(build_include, sizeof(build_include), "%s/%s", cwd, build_include_env) < 0)
				return 1;
		} else {
			strncpy(build_include, build_include_env, sizeof(build_include) - 1);
			build_include[sizeof(build_include) - 1] = '\0';
		}
	} else if (file_exists("/include/build.h")) {
		strcpy(build_include, "/include");
	} else {
		if (pathfmt(build_include, sizeof(build_include), "%s/lib", cwd) < 0)
			return 1;
	}

	char prefix[512];
	if (prefix_arg[0] != '/') {
		if (pathfmt(prefix, sizeof(prefix), "%s/%s", cwd, prefix_arg) < 0)
			return 1;
	} else {
		strncpy(prefix, prefix_arg, sizeof(prefix) - 1);
		prefix[sizeof(prefix) - 1] = '\0';
	}

	if (dir != NULL) {
		if (!is_dir(dir)) {
			fprintf(stderr, "[build] ERROR: not a directory: %s\n", dir);
			return 1;
		}
		if (chdir(dir) < 0) {
			perror(dir);
			return 1;
		}
	}

	if (do_clean) {
		remove_recursive(".build");
		return 0;
	}

	if (file_exists("build.c")) {
		return build_with_buildc(prefix, build_include);
	} else {
		return build_fallback(prefix);
	}
}
