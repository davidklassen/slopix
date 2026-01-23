#include "kprintf.h"
#include "cpu.h"
#include "uart.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)	   __builtin_va_end(ap)

static void reverse(char *buf, int len) {
	int i = 0, j = len - 1;
	while (i < j) {
		char tmp = buf[i];
		buf[i] = buf[j];
		buf[j] = tmp;
		i++;
		j--;
	}
}

static int format_dec(char *buf, int pos, int size, long val, int is_signed) {
	int neg = 0;
	unsigned long uval;
	int start = pos;

	if (is_signed && val < 0) {
		neg = 1;
		uval = -(unsigned long)val;
	} else {
		uval = val;
	}

	if (uval == 0) {
		if (pos < size - 1) {
			buf[pos] = '0';
		}
		return pos + 1;
	}

	while (uval > 0 && pos < size - 1) {
		buf[pos++] = '0' + (uval % 10);
		uval /= 10;
	}

	if (neg && pos < size - 1) {
		buf[pos++] = '-';
	}

	reverse(buf + start, pos - start);
	return pos;
}

static int format_hex(char *buf, int pos, int size, unsigned long val, int width, int upper) {
	const char *hex = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	int start = pos;
	int digits = 0;

	if (val == 0 && width == 0) {
		if (pos < size - 1) {
			buf[pos] = '0';
		}
		return pos + 1;
	}

	unsigned long tmp = val;
	while (tmp > 0 || digits < width) {
		if (pos < size - 1) {
			buf[pos++] = hex[tmp & 0xf];
		}
		tmp >>= 4;
		digits++;
		if (digits >= width && tmp == 0) {
			break;
		}
	}

	reverse(buf + start, pos - start);
	return pos;
}

static int kvsnprintf(char *buf, int size, const char *fmt, va_list ap) {
	int pos = 0;

	while (*fmt && pos < size - 1) {
		if (*fmt != '%') {
			buf[pos++] = *fmt++;
			continue;
		}
		fmt++;

		int is_long = 0;
		if (*fmt == 'l') {
			is_long = 1;
			fmt++;
		}

		switch (*fmt) {
		case 'c':
			if (pos < size - 1) {
				buf[pos++] = (char)va_arg(ap, int);
			}
			break;
		case 's': {
			const char *s = va_arg(ap, const char *);
			if (!s) {
				s = "(null)";
			}
			while (*s && pos < size - 1) {
				buf[pos++] = *s++;
			}
			break;
		}
		case 'd':
			if (is_long) {
				pos = format_dec(buf, pos, size, va_arg(ap, long), 1);
			} else {
				pos = format_dec(buf, pos, size, va_arg(ap, int), 1);
			}
			break;
		case 'u':
			if (is_long) {
				pos = format_dec(buf, pos, size, va_arg(ap, unsigned long), 0);
			} else {
				pos = format_dec(buf, pos, size, va_arg(ap, unsigned int), 0);
			}
			break;
		case 'x':
			if (is_long) {
				pos = format_hex(buf, pos, size, va_arg(ap, unsigned long), 0, 0);
			} else {
				pos = format_hex(buf, pos, size, va_arg(ap, unsigned int), 0, 0);
			}
			break;
		case 'X':
			if (is_long) {
				pos = format_hex(buf, pos, size, va_arg(ap, unsigned long), 0, 1);
			} else {
				pos = format_hex(buf, pos, size, va_arg(ap, unsigned int), 0, 1);
			}
			break;
		case 'p': {
			unsigned long ptr = (unsigned long)va_arg(ap, void *);
			if (pos < size - 1) {
				buf[pos++] = '0';
			}
			if (pos < size - 1) {
				buf[pos++] = 'x';
			}
			pos = format_hex(buf, pos, size, ptr, 16, 0);
			break;
		}
		case '%':
			if (pos < size - 1) {
				buf[pos++] = '%';
			}
			break;
		default:
			if (pos < size - 1) {
				buf[pos++] = '%';
			}
			if (is_long && pos < size - 1) {
				buf[pos++] = 'l';
			}
			if (pos < size - 1) {
				buf[pos++] = *fmt;
			}
			break;
		}
		if (*fmt) {
			fmt++;
		}
	}

	if (size > 0) {
		buf[pos < size ? pos : size - 1] = '\0';
	}

	return pos;
}

int ksnprintf(char *buf, int size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int len = kvsnprintf(buf, size, fmt, ap);
	va_end(ap);
	return len;
}

void kprintf(const char *fmt, ...) {
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	kvsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	uart_puts(buf);
}

__attribute__((noreturn)) void kpanic(const char *fmt, ...) {
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	kvsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	uart_puts(buf);
	uart_puts("\nSystem halted.\n");
	for (;;) {
		wfe();
	}
}
