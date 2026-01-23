#include "kstring.h"

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
