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
	fprintf(stderr, "  clean          Remove .build/ and build/ directories\n");
	fprintf(stderr, "  DIR            Directory to build (default: current)\n");
	exit(1);
}

static int file_exists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

static int is_dir(const char *path) {
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int remove_recursive(const char *path) {
	struct stat st;
	if (lstat(path, &st) < 0) {
		return 0;
	}

	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if (d == NULL) {
			return -1;
		}

		struct dirent *ent;
		while ((ent = readdir(d)) != NULL) {
			if (strcmp(ent->d_name, ".") == 0 ||
			    strcmp(ent->d_name, "..") == 0) {
				continue;
			}
			char child[512];
			snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
			if (remove_recursive(child) < 0) {
				closedir(d);
				return -1;
			}
		}
		closedir(d);
		return rmdir(path);
	} else {
		return unlink(path);
	}
}

static int mkdir_p(const char *path) {
	char buf[256];
	strncpy(buf, path, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	for (char *p = buf + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir(buf, 0755) < 0) {
				struct stat st;
				if (!(stat(buf, &st) == 0 && S_ISDIR(st.st_mode))) {
					return -1;
				}
			}
			*p = '/';
		}
	}
	if (mkdir(buf, 0755) < 0) {
		struct stat st;
		if (!(stat(buf, &st) == 0 && S_ISDIR(st.st_mode))) {
			return -1;
		}
	}
	return 0;
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

static const char *get_env_or(const char *name, const char *def) {
	const char *val = getenv(name);
	if (val == NULL || val[0] == '\0') {
		return def;
	}
	return val;
}

static const char *basename_c(const char *path) {
	const char *last = strrchr(path, '/');
	return last ? last + 1 : path;
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
	char incflag[256];
	snprintf(incflag, sizeof(incflag), "-I%s", build_include);

	char *compile_argv[] = {"cc", incflag, "build.c", "-o", ".build/build", NULL};
	int ret = run_cmd(compile_argv);
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

	const char *cc = get_env_or("CC", "cc");
	const char *as = get_env_or("AS", "as");
	const char *ld = get_env_or("LD", "ld");
	const char *include_path = get_env_or("INCLUDE_PATH", "");
	const char *lib_path = get_env_or("LIB_PATH", "");

	if (mkdir_p(".build/obj") < 0) {
		fprintf(stderr, "[build] ERROR: cannot create .build/obj/\n");
		free(src);
		free(dirname);
		return 1;
	}
	if (mkdir_p(prefix) < 0) {
		fprintf(stderr, "[build] ERROR: cannot create %s/\n", prefix);
		free(src);
		free(dirname);
		return 1;
	}

	size_t srclen = strlen(src);
	char base[256];
	snprintf(base, sizeof(base), "%.*s", (int)(srclen - 2), src);

	char sfile[256], ofile[256], outfile[256];
	snprintf(sfile, sizeof(sfile), ".build/obj/%s.s", base);
	snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", base);
	snprintf(outfile, sizeof(outfile), "%s/%s", prefix, dirname);

	int ret;

	if (include_path[0]) {
		char incflag[256];
		snprintf(incflag, sizeof(incflag), "-I%s", include_path);
		char *argv[] = {(char *)cc, incflag, "-S", src, "-o", sfile, NULL};
		ret = run_cmd(argv);
	} else {
		char *argv[] = {(char *)cc, "-S", src, "-o", sfile, NULL};
		ret = run_cmd(argv);
	}
	if (ret != 0) {
		free(src);
		free(dirname);
		return ret;
	}

	{
		char *argv[] = {(char *)as, "-o", ofile, sfile, NULL};
		ret = run_cmd(argv);
	}
	if (ret != 0) {
		free(src);
		free(dirname);
		return ret;
	}

	if (lib_path[0]) {
		char libcpath[256];
		snprintf(libcpath, sizeof(libcpath), "%s/libc.a", lib_path);
		char *argv[] = {(char *)ld, "-o", outfile, ofile, libcpath, NULL};
		ret = run_cmd(argv);
	} else {
		// Host build: system libc is linked implicitly via LD=cc
		char *argv[] = {(char *)ld, "-o", outfile, ofile, NULL};
		ret = run_cmd(argv);
	}

	free(src);
	free(dirname);
	return ret;
}

int main(int argc, char **argv) {
	const char *prefix_arg = "build/bin";
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
	const char *build_include_env = get_env_or("BUILD_INCLUDE", "lib");
	char build_include[512];
	if (build_include_env[0] != '/') {
		snprintf(build_include, sizeof(build_include), "%s/%s", cwd, build_include_env);
	} else {
		strncpy(build_include, build_include_env, sizeof(build_include) - 1);
		build_include[sizeof(build_include) - 1] = '\0';
	}

	char prefix[512];
	if (prefix_arg[0] != '/') {
		snprintf(prefix, sizeof(prefix), "%s/%s", cwd, prefix_arg);
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
		remove_recursive("build");
		return 0;
	}

	if (file_exists("build.c")) {
		return build_with_buildc(prefix, build_include);
	} else {
		return build_fallback(prefix);
	}
}
