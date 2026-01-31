/*
 * build.h - Header-only build library for slopix
 *
 * Heavily inspired by nob.h by Alexey Kutepov (tsoding)
 * https://github.com/tsoding/nob.h
 *
 * nob.h is Public Domain. This file is part of slopix.
 *
 * Include this header in build.c files. Define BUILD_IMPLEMENTATION
 * before including in exactly one source file to get the implementation.
 *
 * Environment variables:
 *   CC            C compiler
 *   AS            Assembler
 *   LD            Linker
 *   AR            Archive tool
 *   BUILD         Path to build tool (default: /bin/build)
 *   INCLUDE_PATH  Header search path
 *   LIB_PATH      Library path (if set, links libc.a)
 */
#ifndef BUILD_H
#define BUILD_H

#include <stdarg.h>
#include <stddef.h>

typedef struct {
	const char **items;
	int count;
	int cap;
} Cmd;

void cmd_append(Cmd *c, ...);
void cmd_reset(Cmd *c);
int cmd_run(Cmd *c);

const char *get_bin_prefix(void);

int build_subdir(const char *dir);

int compile(const char *src);
int assemble(const char *src);
int link_objs(const char *out, const char **objs);
int archive_objs(const char *out, const char **objs);

int mkdir_p(const char *path);
int file_exists(const char *path);
int move_recursive(const char *src, const char *dst);
int remove_recursive(const char *path);

void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif // BUILD_H

#ifdef BUILD_IMPLEMENTATION

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

void log_info(const char *fmt, ...) {
	fprintf(stderr, "[build] ");
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

void log_error(const char *fmt, ...) {
	fprintf(stderr, "[build] ERROR: ");
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

void cmd_append(Cmd *c, ...) {
	va_list ap;
	va_start(ap, c);
	const char *arg;
	while ((arg = va_arg(ap, const char *)) != NULL) {
		if (c->count >= c->cap) {
			int newcap = c->cap == 0 ? 16 : c->cap * 2;
			const char **newitems = realloc(c->items, newcap * sizeof(const char *));
			if (newitems == NULL) {
				log_error("out of memory");
				va_end(ap);
				return;
			}
			c->items = newitems;
			c->cap = newcap;
		}
		c->items[c->count++] = arg;
	}
	va_end(ap);
}

void cmd_reset(Cmd *c) {
	free(c->items);
	c->items = NULL;
	c->count = 0;
	c->cap = 0;
}

static char *cmd_to_string(Cmd *c) {
	size_t total = 0;
	for (int i = 0; i < c->count; i++) {
		total += strlen(c->items[i]) + 1;
	}

	char *buf = malloc(total + 1);
	if (buf == NULL) {
		return NULL;
	}

	char *p = buf;
	for (int i = 0; i < c->count; i++) {
		size_t len = strlen(c->items[i]);
		memcpy(p, c->items[i], len);
		p += len;
		*p++ = ' ';
	}
	if (p > buf) {
		p[-1] = '\0';
	} else {
		*p = '\0';
	}
	return buf;
}

int cmd_run(Cmd *c) {
	if (c->count == 0) {
		return 0;
	}

	char *cmdstr = cmd_to_string(c);
	if (cmdstr) {
		log_info("%s", cmdstr);
		free(cmdstr);
	}

	int pid = fork();
	if (pid < 0) {
		log_error("fork failed");
		return 1;
	}
	if (pid == 0) {
		char **argv = malloc((c->count + 1) * sizeof(char *));
		if (argv == NULL) {
			perror("malloc");
			exit(127);
		}
		for (int i = 0; i < c->count; i++) {
			argv[i] = (char *)c->items[i];
		}
		argv[c->count] = NULL;
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

static int mkdir_single(const char *path) {
	if (mkdir(path, 0755) < 0) {
		struct stat st;
		if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
			return 0;
		}
		return -1;
	}
	return 0;
}

int mkdir_p(const char *path) {
	char buf[256];
	strncpy(buf, path, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	for (char *p = buf + 1; *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (mkdir_single(buf) < 0) {
				return -1;
			}
			*p = '/';
		}
	}
	return mkdir_single(buf);
}

int file_exists(const char *path) {
	struct stat st;
	return stat(path, &st) == 0;
}

int remove_recursive(const char *path) {
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
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
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

static int copy_file(const char *src, const char *dst) {
	int sfd = open(src, O_RDONLY);
	if (sfd < 0) {
		return -1;
	}

	struct stat st;
	if (fstat(sfd, &st) < 0) {
		close(sfd);
		return -1;
	}

	int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
	if (dfd < 0) {
		close(sfd);
		return -1;
	}

	char buf[4096];
	ssize_t n;
	while ((n = read(sfd, buf, sizeof(buf))) > 0) {
		ssize_t written = 0;
		while (written < n) {
			ssize_t w = write(dfd, buf + written, n - written);
			if (w < 0) {
				close(sfd);
				close(dfd);
				return -1;
			}
			written += w;
		}
	}

	close(sfd);
	close(dfd);
	return n < 0 ? -1 : 0;
}

int move_recursive(const char *src, const char *dst) {
	if (rename(src, dst) == 0) {
		return 0;
	}

	struct stat st;
	if (lstat(src, &st) < 0) {
		return -1;
	}

	if (S_ISDIR(st.st_mode)) {
		if (mkdir_p(dst) < 0) {
			return -1;
		}

		DIR *d = opendir(src);
		if (d == NULL) {
			return -1;
		}

		struct dirent *ent;
		while ((ent = readdir(d)) != NULL) {
			if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
				continue;
			}
			char srcchild[512], dstchild[512];
			snprintf(srcchild, sizeof(srcchild), "%s/%s", src, ent->d_name);
			snprintf(dstchild, sizeof(dstchild), "%s/%s", dst, ent->d_name);
			if (move_recursive(srcchild, dstchild) < 0) {
				closedir(d);
				return -1;
			}
		}
		closedir(d);
		return rmdir(src);
	} else {
		if (copy_file(src, dst) < 0) {
			return -1;
		}
		return unlink(src);
	}
}

static const char *get_env_or(const char *name, const char *def) {
	const char *val = getenv(name);
	if (val == NULL || val[0] == '\0') {
		return def;
	}
	return val;
}

const char *get_bin_prefix(void) {
	return get_env_or("BUILD_PREFIX", ".build/out/bin");
}

int build_subdir(const char *dir) {
	char origdir[512];
	if (getcwd(origdir, sizeof(origdir)) == NULL) {
		log_error("getcwd failed");
		return 1;
	}

	if (chdir(dir) < 0) {
		log_error("chdir to '%s' failed", dir);
		return 1;
	}

	const char *build = get_env_or("BUILD", "/bin/build");

	Cmd cmd = {0};
	cmd_append(&cmd, build, NULL);
	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	if (ret != 0) {
		chdir(origdir);
		return ret;
	}

	if (file_exists(".build/out")) {
		char dstout[512];
		snprintf(dstout, sizeof(dstout), "%s/.build/out", origdir);

		DIR *d = opendir(".build/out");
		if (d != NULL) {
			struct dirent *ent;
			while ((ent = readdir(d)) != NULL) {
				if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
					continue;
				}
				char src[512], dst[512];
				snprintf(src, sizeof(src), ".build/out/%s", ent->d_name);
				snprintf(dst, sizeof(dst), "%s/%s", dstout, ent->d_name);
				mkdir_p(dstout);
				if (move_recursive(src, dst) < 0) {
					log_error("failed to move %s to %s", src, dst);
				}
			}
			closedir(d);
		}
	}

	remove_recursive(".build");

	if (chdir(origdir) < 0) {
		log_error("chdir back to '%s' failed", origdir);
		return 1;
	}

	return 0;
}

static const char *basename_c(const char *path) {
	const char *last = strrchr(path, '/');
	return last ? last + 1 : path;
}

int compile(const char *src) {
	const char *cc = get_env_or("CC", "cc");
	const char *as = get_env_or("AS", "as");
	const char *include_path = getenv("INCLUDE_PATH");
	if (include_path == NULL || include_path[0] == '\0') {
		if (file_exists("/src/libc/include")) {
			include_path = "/src/libc/include";
		} else {
			include_path = "";
		}
	}

	if (mkdir_p(".build/obj") < 0) {
		log_error("cannot create .build/obj");
		return 1;
	}

	const char *base = basename_c(src);
	char sfile[256], ofile[256];
	size_t len = strlen(base);
	if (len > 2 && strcmp(base + len - 2, ".c") == 0) {
		snprintf(sfile, sizeof(sfile), ".build/obj/%.*s.s", (int)(len - 2), base);
		snprintf(ofile, sizeof(ofile), ".build/obj/%.*s.o", (int)(len - 2), base);
	} else {
		snprintf(sfile, sizeof(sfile), ".build/obj/%s.s", base);
		snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", base);
	}

	Cmd cmd = {0};
	cmd_append(&cmd, cc, NULL);
	if (include_path[0]) {
		static char incflag[256];
		snprintf(incflag, sizeof(incflag), "-I%s", include_path);
		cmd_append(&cmd, incflag, NULL);
	}
	cmd_append(&cmd, "-S", src, "-o", sfile, NULL);

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

int assemble(const char *src) {
	const char *as = get_env_or("AS", "as");

	if (mkdir_p(".build/obj") < 0) {
		log_error("cannot create .build/obj");
		return 1;
	}

	const char *base = basename_c(src);
	char ofile[256];
	size_t len = strlen(base);
	if (len > 2 && strcmp(base + len - 2, ".S") == 0) {
		snprintf(ofile, sizeof(ofile), ".build/obj/%.*s.o", (int)(len - 2), base);
	} else {
		snprintf(ofile, sizeof(ofile), ".build/obj/%s.o", base);
	}

	Cmd cmd = {0};
	cmd_append(&cmd, as, "-o", ofile, src, NULL);
	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

static char objbufs[32][256];
static int objbuf_idx = 0;

static const char *make_objpath(const char *base) {
	snprintf(objbufs[objbuf_idx], sizeof(objbufs[0]), ".build/obj/%s.o", base);
	const char *result = objbufs[objbuf_idx];
	objbuf_idx = (objbuf_idx + 1) % 32;
	return result;
}

int link_objs(const char *out, const char **objs) {
	const char *ld = get_env_or("LD", "ld");
	const char *lib_path = getenv("LIB_PATH");
	if (lib_path == NULL || lib_path[0] == '\0') {
		if (file_exists("/lib/libc.a")) {
			lib_path = "/lib";
		} else {
			lib_path = "";
		}
	}

	Cmd cmd = {0};
	cmd_append(&cmd, ld, "-o", out, NULL);

	for (int i = 0; objs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(objs[i]), NULL);
	}

	if (lib_path[0]) {
		static char libcpath[256];
		snprintf(libcpath, sizeof(libcpath), "%s/libc.a", lib_path);
		cmd_append(&cmd, libcpath, NULL);
	}

	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

int archive_objs(const char *out, const char **objs) {
	const char *ar = get_env_or("AR", "ar");

	Cmd cmd = {0};
	cmd_append(&cmd, ar, "rcs", out, NULL);

	for (int i = 0; objs[i] != NULL; i++) {
		cmd_append(&cmd, make_objpath(objs[i]), NULL);
	}

	int ret = cmd_run(&cmd);
	cmd_reset(&cmd);

	return ret;
}

#endif // BUILD_IMPLEMENTATION
