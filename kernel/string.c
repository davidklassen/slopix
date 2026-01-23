#include "string.h"

unsigned int strlen(const char *s) {
	unsigned int n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, unsigned int n) {
	while (n > 0 && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	if (n == 0) {
		return 0;
	}
	return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strncpy(char *dst, const char *src, unsigned int n) {
	char *d = dst;
	while (n > 0 && *src) {
		*d++ = *src++;
		n--;
	}
	while (n > 0) {
		*d++ = 0;
		n--;
	}
	return dst;
}

void *memmove(void *dst, const void *src, unsigned int n) {
	char *d = dst;
	const char *s = src;

	if (s < d && d < s + n) {
		s += n;
		d += n;
		while (n-- > 0) {
			*--d = *--s;
		}
	} else {
		while (n-- > 0) {
			*d++ = *s++;
		}
	}
	return dst;
}

void *memset(void *s, int c, unsigned int n) {
	unsigned char *p = s;
	while (n-- > 0) {
		*p++ = (unsigned char)c;
	}
	return s;
}

void *memcpy(void *dst, const void *src, unsigned int n) {
	char *d = dst;
	const char *s = src;
	while (n-- > 0) {
		*d++ = *s++;
	}
	return dst;
}

char *strchr(const char *s, int c) {
	while (*s != (char)c) {
		if (*s == '\0') {
			return 0;
		}
		s++;
	}
	return (char *)s;
}

char *strstr(const char *haystack, const char *needle) {
	if (!*needle) {
		return (char *)haystack;
	}
	for (; *haystack; haystack++) {
		const char *h = haystack;
		const char *n = needle;
		while (*h && *n && *h == *n) {
			h++;
			n++;
		}
		if (!*n) {
			return (char *)haystack;
		}
	}
	return 0;
}
