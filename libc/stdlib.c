#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define ATEXIT_MAX 32
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
	char cmdline[512];
	int pos = 0;

	for (int i = 0; argv[i]; i++) {
		if (i > 0 && pos < 511) {
			cmdline[pos++] = ' ';
		}
		const char *arg = argv[i];
		while (*arg && pos < 511) {
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
