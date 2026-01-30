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

static void print_int(int n) {
	char buf[16];
	itoa(n, buf);
	puts(buf);
}

static void print_long(long n) {
	char buf[24];
	char *p = buf + sizeof(buf) - 1;
	*p = '\0';
	int neg = n < 0;
	unsigned long u = neg ? -n : n;
	do {
		*--p = '0' + (u % 10);
		u /= 10;
	} while (u);
	if (neg) {
		*--p = '-';
	}
	puts(p);
}

static void print_hex(unsigned int n) {
	char buf[16];
	char *digits = "0123456789abcdef";
	int i = 0;

	if (n == 0) {
		write(1, "0", 1);
		return;
	}

	while (n > 0) {
		buf[i++] = digits[n & 0xf];
		n >>= 4;
	}
	while (i > 0) {
		write(1, &buf[--i], 1);
	}
}

static void print_int_width(int n, int width) {
	char buf[16];
	itoa(n, buf);
	int len = strlen(buf);
	while (len < width) {
		write(1, " ", 1);
		len++;
	}
	puts(buf);
}

static void print_str_width(const char *s, int width) {
	if (!s) {
		s = "(null)";
	}
	int len = strlen(s);
	while (len < width) {
		write(1, " ", 1);
		len++;
	}
	puts(s);
}

int printf(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);

	int count = 0;
	while (*fmt) {
		if (*fmt == '%' && *(fmt + 1)) {
			fmt++;
			int width = 0;
			int is_long = 0;
			while (*fmt >= '0' && *fmt <= '9') {
				width = width * 10 + (*fmt - '0');
				fmt++;
			}
			if (*fmt == 'l') {
				is_long = 1;
				fmt++;
			}
			switch (*fmt) {
			case 'd':
				if (is_long) {
					print_long(va_arg(ap, long));
				} else if (width > 0) {
					print_int_width(va_arg(ap, int), width);
				} else {
					print_int(va_arg(ap, int));
				}
				break;
			case 'u':
				if (is_long) {
					print_long(va_arg(ap, long));
				} else {
					print_int(va_arg(ap, int));
				}
				break;
			case 's': {
				const char *s = va_arg(ap, const char *);
				if (width > 0) {
					print_str_width(s, width);
				} else if (s) {
					count += puts(s);
				}
				break;
			}
			case 'x':
				print_hex(va_arg(ap, unsigned int));
				break;
			case 'c': {
				char c = va_arg(ap, int);
				write(1, &c, 1);
				count++;
				break;
			}
			case '%':
				write(1, "%", 1);
				count++;
				break;
			default:
				write(1, "%", 1);
				write(1, fmt, 1);
				count += 2;
			}
		} else {
			write(1, fmt, 1);
			count++;
		}
		fmt++;
	}

	va_end(ap);
	return count;
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
