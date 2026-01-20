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

int printf(const char *fmt, ...) {
	__builtin_va_list ap;
	__builtin_va_start(ap, fmt);

	int count = 0;
	while (*fmt) {
		if (*fmt == '%' && *(fmt + 1)) {
			fmt++;
			switch (*fmt) {
			case 'd':
				print_int(__builtin_va_arg(ap, int));
				break;
			case 's': {
				const char *s = __builtin_va_arg(ap, const char *);
				if (s) {
					count += puts(s);
				}
				break;
			}
			case 'x':
				print_hex(__builtin_va_arg(ap, unsigned int));
				break;
			case 'c': {
				char c = __builtin_va_arg(ap, int);
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

	__builtin_va_end(ap);
	return count;
}
