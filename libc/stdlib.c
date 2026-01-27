#include <stdlib.h>
#include <ctype.h>

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
