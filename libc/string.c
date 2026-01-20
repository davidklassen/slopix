#include <string.h>

size_t strlen(const char *s) {
	size_t n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

void *memset(void *s, int c, size_t n) {
	unsigned char *p = s;
	while (n--) {
		*p++ = c;
	}
	return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
	unsigned char *d = dest;
	const unsigned char *s = src;
	while (n--) {
		*d++ = *s++;
	}
	return dest;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int atoi(const char *s) {
	int n = 0;
	int neg = 0;
	while (*s == ' ') {
		s++;
	}
	if (*s == '-') {
		neg = 1;
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		n = n * 10 + (*s - '0');
		s++;
	}
	return neg ? -n : n;
}

int itoa(int n, char *buf) {
	char tmp[16];
	int i = 0;
	int neg = 0;

	if (n < 0) {
		neg = 1;
		n = -n;
	}
	do {
		tmp[i++] = '0' + (n % 10);
		n /= 10;
	} while (n > 0);

	int len = 0;
	if (neg) {
		buf[len++] = '-';
	}
	while (i > 0) {
		buf[len++] = tmp[--i];
	}
	buf[len] = '\0';
	return len;
}
