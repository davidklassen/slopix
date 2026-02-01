#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int puts(const char *s) {
	int len = strlen(s);
	write(1, s, len);
	return len;
}

void perror(const char *s) {
	if (s && *s) {
		write(2, s, strlen(s));
		write(2, ": ", 2);
	}
	const char *msg = strerror(errno);
	write(2, msg, strlen(msg));
	write(2, "\n", 1);
}
