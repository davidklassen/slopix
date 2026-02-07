#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int exec(const char *cmdline);

#ifndef ATEXIT_MAX
#define ATEXIT_MAX 32
#endif
static void (*atexit_funcs[ATEXIT_MAX])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
	if (atexit_count >= ATEXIT_MAX) {
		return -1;
	}
	atexit_funcs[atexit_count++] = func;
	return 0;
}

void exit(int status) {
	while (atexit_count > 0) {
		atexit_funcs[--atexit_count]();
	}
	_exit(status);
}

static unsigned int rand_state = 1;

static unsigned int simple_rand(void) {
	rand_state = rand_state * 1103515245 + 12345;
	return (rand_state >> 16) & 0x7fff;
}

int mkstemp(char *templ) {
	size_t len = strlen(templ);
	if (len < 6) {
		return -1;
	}

	char *suffix = templ + len - 6;
	for (int i = 0; i < 6; i++) {
		if (suffix[i] != 'X') {
			return -1;
		}
	}

	static const char chars[] =
	    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

	for (int attempt = 0; attempt < 100; attempt++) {
		for (int i = 0; i < 6; i++) {
			suffix[i] = chars[simple_rand() % 62];
		}

		int fd = open(templ, O_RDWR | O_CREAT | O_EXCL);
		if (fd >= 0) {
			return fd;
		}
	}

	return -1;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
	const char *s = nptr;
	unsigned long result = 0;
	int neg = 0;

	while (isspace(*s)) {
		s++;
	}

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	if (base == 0) {
		if (*s == '0') {
			s++;
			if (*s == 'x' || *s == 'X') {
				base = 16;
				s++;
			} else {
				base = 8;
			}
		} else {
			base = 10;
		}
	} else if (base == 16 && *s == '0' && (s[1] == 'x' || s[1] == 'X')) {
		s += 2;
	}

	while (*s) {
		int digit;
		if (isdigit(*s)) {
			digit = *s - '0';
		} else if (*s >= 'a' && *s <= 'z') {
			digit = *s - 'a' + 10;
		} else if (*s >= 'A' && *s <= 'Z') {
			digit = *s - 'A' + 10;
		} else {
			break;
		}

		if (digit >= base) {
			break;
		}

		result = result * base + digit;
		s++;
	}

	if (endptr) {
		*endptr = (char *)s;
	}

	return neg ? -result : result;
}

long strtol(const char *nptr, char **endptr, int base) {
	return (long)strtoul(nptr, endptr, base);
}

int atoi(const char *s) {
	return (int)strtol(s, NULL, 10);
}

long double strtold(const char *nptr, char **endptr) {
	const char *s = nptr;
	long double result = 0;
	long double frac = 0;
	int neg = 0;
	int exp = 0;
	int exp_neg = 0;

	while (isspace(*s)) {
		s++;
	}

	if (*s == '-') {
		neg = 1;
		s++;
	} else if (*s == '+') {
		s++;
	}

	while (isdigit(*s)) {
		result = result * 10 + (*s - '0');
		s++;
	}

	if (*s == '.') {
		s++;
		long double divisor = 10;
		while (isdigit(*s)) {
			frac += (*s - '0') / divisor;
			divisor *= 10;
			s++;
		}
	}

	result += frac;

	if (*s == 'e' || *s == 'E') {
		s++;
		if (*s == '-') {
			exp_neg = 1;
			s++;
		} else if (*s == '+') {
			s++;
		}
		while (isdigit(*s)) {
			exp = exp * 10 + (*s - '0');
			s++;
		}
		long double multiplier = 1;
		for (int i = 0; i < exp; i++) {
			multiplier *= 10;
		}
		if (exp_neg) {
			result /= multiplier;
		} else {
			result *= multiplier;
		}
	}

	if (endptr) {
		*endptr = (char *)s;
	}

	return neg ? -result : result;
}

int access(const char *path, int mode) {
	(void)mode;
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		return -1;
	}
	close(fd);
	return 0;
}

int isatty(int fd) {
	return fd >= 0 && fd <= 2;
}

extern int _wait_syscall(void);

int wait(int *wstatus) {
	int ret = _wait_syscall();
	if (ret < 0) {
		return -1;
	}
	int pid = ret >> 16;
	int status = ret & 0xffff;
	if (wstatus) {
		*wstatus = status;
	}
	return pid;
}

int execvp(const char *file, char *const argv[]) {
	char cmdline[1024];
	int pos = 0;

	// If file has no '/', search /bin/ (simple PATH behavior)
	const char *path = file;
	char pathbuf[256];
	if (strchr(file, '/') == NULL) {
		snprintf(pathbuf, sizeof(pathbuf), "/bin/%s", file);
		path = pathbuf;
	}

	// Build cmdline with resolved path as first arg
	const char *p = path;
	while (*p && pos < 1023) {
		cmdline[pos++] = *p++;
	}

	// Append remaining args
	for (int i = 1; argv[i]; i++) {
		if (pos < 1023) {
			cmdline[pos++] = ' ';
		}
		const char *arg = argv[i];
		while (*arg && pos < 1023) {
			cmdline[pos++] = *arg++;
		}
	}
	cmdline[pos] = '\0';

	return exec(cmdline);
}

void _assert_fail(const char *expr, const char *file, int line) {
	write(2, "assert failed: ", 15);
	write(2, expr, strlen(expr));
	write(2, " at ", 4);
	write(2, file, strlen(file));
	write(2, "\n", 1);
	_exit(1);
}

#define ENV_MAX 64
static char *environ_storage[ENV_MAX];
static int environ_count = 0;

char *getenv(const char *name) {
	if (name == NULL || name[0] == '\0') {
		return NULL;
	}
	size_t namelen = strlen(name);
	for (int i = 0; i < environ_count; i++) {
		if (environ_storage[i] != NULL &&
		    strncmp(environ_storage[i], name, namelen) == 0 &&
		    environ_storage[i][namelen] == '=') {
			return environ_storage[i] + namelen + 1;
		}
	}
	return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
	if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
		return -1;
	}
	size_t namelen = strlen(name);
	size_t vallen = strlen(value);

	for (int i = 0; i < environ_count; i++) {
		if (environ_storage[i] != NULL &&
		    strncmp(environ_storage[i], name, namelen) == 0 &&
		    environ_storage[i][namelen] == '=') {
			if (!overwrite) {
				return 0;
			}
			free(environ_storage[i]);
			char *entry = malloc(namelen + 1 + vallen + 1);
			if (entry == NULL) {
				return -1;
			}
			memcpy(entry, name, namelen);
			entry[namelen] = '=';
			memcpy(entry + namelen + 1, value, vallen + 1);
			environ_storage[i] = entry;
			return 0;
		}
	}

	if (environ_count >= ENV_MAX) {
		return -1;
	}
	char *entry = malloc(namelen + 1 + vallen + 1);
	if (entry == NULL) {
		return -1;
	}
	memcpy(entry, name, namelen);
	entry[namelen] = '=';
	memcpy(entry + namelen + 1, value, vallen + 1);
	environ_storage[environ_count++] = entry;
	return 0;
}

int unsetenv(const char *name) {
	if (name == NULL || name[0] == '\0' || strchr(name, '=') != NULL) {
		return -1;
	}
	size_t namelen = strlen(name);
	for (int i = 0; i < environ_count; i++) {
		if (environ_storage[i] != NULL &&
		    strncmp(environ_storage[i], name, namelen) == 0 &&
		    environ_storage[i][namelen] == '=') {
			free(environ_storage[i]);
			environ_storage[i] = environ_storage[environ_count - 1];
			environ_storage[environ_count - 1] = NULL;
			environ_count--;
			return 0;
		}
	}
	return 0;
}

extern int _mkdir_syscall(const char *path);
int mkdir(const char *path, mode_t mode) {
	(void)mode;
	return _mkdir_syscall(path);
}

extern int _waitpid_syscall(int pid, int options);
int waitpid(int pid, int *wstatus, int options) {
	int ret = _waitpid_syscall(pid, options);
	if (ret < 0) {
		return -1;
	}
	if (wstatus) {
		*wstatus = ret & 0xffff;
	}
	return ret >> 16;
}

static void swap(char *a, char *b, size_t size) {
	char buf[256];
	if (size <= sizeof(buf)) {
		memcpy(buf, a, size);
		memcpy(a, b, size);
		memcpy(b, buf, size);
	} else {
		for (size_t i = 0; i < size; i++) {
			char tmp = a[i];
			a[i] = b[i];
			b[i] = tmp;
		}
	}
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	if (nmemb < 2)
		return;

	struct {
		size_t lo, hi;
	} stack[64];
	int top = 0;

	stack[top].lo = 0;
	stack[top].hi = nmemb - 1;
	top++;

	char *arr = base;

	while (top > 0) {
		top--;
		size_t lo = stack[top].lo;
		size_t hi = stack[top].hi;

		while (lo < hi) {
			// Median-of-three pivot
			size_t mid = lo + (hi - lo) / 2;
			if (compar(arr + mid * size, arr + lo * size) < 0)
				swap(arr + mid * size, arr + lo * size, size);
			if (compar(arr + hi * size, arr + lo * size) < 0)
				swap(arr + hi * size, arr + lo * size, size);
			if (compar(arr + mid * size, arr + hi * size) < 0)
				swap(arr + mid * size, arr + hi * size, size);
			// Pivot is now at arr[hi]

			// Hoare-like partition with Lomuto scheme
			char *pivot = arr + hi * size;
			size_t i = lo;
			for (size_t j = lo; j < hi; j++) {
				if (compar(arr + j * size, pivot) < 0) {
					swap(arr + i * size, arr + j * size, size);
					i++;
				}
			}
			swap(arr + i * size, arr + hi * size, size);

			// Push larger partition, loop on smaller
			size_t left_size = (i > lo) ? i - lo : 0;
			size_t right_size = (hi > i) ? hi - i : 0;

			if (left_size > right_size) {
				if (i > lo + 1) {
					stack[top].lo = lo;
					stack[top].hi = i - 1;
					top++;
				}
				lo = i + 1;
			} else {
				if (i + 1 < hi) {
					stack[top].lo = i + 1;
					stack[top].hi = hi;
					top++;
				}
				hi = (i > 0) ? i - 1 : 0;
				if (i == 0)
					break;
			}
		}
	}
}

int lstat(const char *path, struct stat *st) {
	return stat(path, st);
}

int rmdir(const char *path) {
	return unlink(path);
}
