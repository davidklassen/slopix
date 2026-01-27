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
