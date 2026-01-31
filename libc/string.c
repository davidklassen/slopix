#include <ctype.h>
#include <stdlib.h>
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

void *memmove(void *dest, const void *src, size_t n) {
	char *d = dest;
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
	return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
	const unsigned char *p1 = s1;
	const unsigned char *p2 = s2;
	while (n--) {
		if (*p1 != *p2) {
			return *p1 - *p2;
		}
		p1++;
		p2++;
	}
	return 0;
}

void *memchr(const void *s, int c, size_t n) {
	const unsigned char *p = s;
	while (n--) {
		if (*p == (unsigned char)c) {
			return (void *)p;
		}
		p++;
	}
	return 0;
}

int strcmp(const char *s1, const char *s2) {
	while (*s1 && *s1 == *s2) {
		s1++;
		s2++;
	}
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
	while (n > 0 && *s1 && *s1 == *s2) {
		s1++;
		s2++;
		n--;
	}
	if (n == 0) {
		return 0;
	}
	return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
	char *d = dest;
	while ((*d++ = *src++))
		;
	return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
	char *d = dest;
	while (n > 0 && *src) {
		*d++ = *src++;
		n--;
	}
	while (n > 0) {
		*d++ = '\0';
		n--;
	}
	return dest;
}

char *strcat(char *dest, const char *src) {
	char *d = dest;
	while (*d) {
		d++;
	}
	while ((*d++ = *src++))
		;
	return dest;
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
	if (*needle == '\0') {
		return (char *)haystack;
	}
	for (; *haystack; haystack++) {
		if (*haystack == *needle) {
			const char *h = haystack;
			const char *n = needle;
			while (*h && *n && *h == *n) {
				h++;
				n++;
			}
			if (*n == '\0') {
				return (char *)haystack;
			}
		}
	}
	return 0;
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

char *strrchr(const char *s, int c) {
	const char *last = 0;
	while (*s) {
		if (*s == (char)c) {
			last = s;
		}
		s++;
	}
	if (c == '\0') {
		return (char *)s;
	}
	return (char *)last;
}

char *strdup(const char *s) {
	size_t len = strlen(s) + 1;
	char *dup = malloc(len);
	if (dup) {
		memcpy(dup, s, len);
	}
	return dup;
}

char *strndup(const char *s, size_t n) {
	size_t len = strlen(s);
	if (len > n) {
		len = n;
	}
	char *dup = malloc(len + 1);
	if (dup) {
		memcpy(dup, s, len);
		dup[len] = '\0';
	}
	return dup;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
	char *s = str ? str : *saveptr;
	if (!s) {
		return 0;
	}

	while (*s && strchr(delim, *s)) {
		s++;
	}

	if (*s == '\0') {
		*saveptr = 0;
		return 0;
	}

	char *token_start = s;

	while (*s && !strchr(delim, *s)) {
		s++;
	}

	if (*s) {
		*s = '\0';
		*saveptr = s + 1;
	} else {
		*saveptr = 0;
	}

	return token_start;
}

static char *strtok_state;

char *strtok(char *str, const char *delim) {
	return strtok_r(str, delim, &strtok_state);
}

int strcasecmp(const char *s1, const char *s2) {
	while (*s1 && tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
		s1++;
		s2++;
	}
	return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
	while (n > 0 && *s1 &&
	       tolower((unsigned char)*s1) == tolower((unsigned char)*s2)) {
		s1++;
		s2++;
		n--;
	}
	if (n == 0) {
		return 0;
	}
	return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
