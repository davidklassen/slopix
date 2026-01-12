#include "printf.h"
#include "uart.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_end(ap) __builtin_va_end(ap)

static void print_int(int value) {
    if (value < 0) {
        uart_putchar('-');
        value = -value;
    }

    if (value == 0) {
        uart_putchar('0');
        return;
    }

    char buf[32];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_putchar(buf[--i]);
    }
}

static void print_hex(unsigned int value, int prefix) {
    const char hex[] = "0123456789abcdef";
    if (prefix) {
        uart_puts("0x");
    }

    int started = 0;
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            uart_putchar(hex[digit]);
            started = 1;
        }
    }
}

static void print_hex_long(unsigned long value, int prefix) {
    const char hex[] = "0123456789abcdef";
    if (prefix) {
        uart_puts("0x");
    }

    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            uart_putchar(hex[digit]);
            started = 1;
        }
    }
}

static void print_ulong(unsigned long value) {
    if (value == 0) {
        uart_putchar('0');
        return;
    }

    char buf[32];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        uart_putchar(buf[--i]);
    }
}

static void print_string(const char *s) {
    if (s == 0) {
        uart_puts("(null)");
    } else {
        uart_puts(s);
    }
}

void printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            // Check for '#' flag (alternate form - adds 0x prefix for hex)
            int alt_form = 0;
            if (*fmt == '#') {
                alt_form = 1;
                fmt++;
            }
            // Check for 'l' length modifier
            if (*fmt == 'l') {
                fmt++;
                switch (*fmt) {
                    case 'u':
                        print_ulong(va_arg(args, unsigned long));
                        break;
                    case 'x':
                        print_hex_long(va_arg(args, unsigned long), alt_form);
                        break;
                    case 'd':
                        // For simplicity, treat %ld as unsigned long
                        print_ulong(va_arg(args, unsigned long));
                        break;
                    default:
                        uart_putchar('%');
                        if (alt_form) uart_putchar('#');
                        uart_putchar('l');
                        uart_putchar(*fmt);
                        break;
                }
            } else {
                switch (*fmt) {
                    case 's':
                        print_string(va_arg(args, const char *));
                        break;
                    case 'd':
                        print_int(va_arg(args, int));
                        break;
                    case 'x':
                        print_hex(va_arg(args, unsigned int), alt_form);
                        break;
                    case 'p':
                        // %p always gets 0x prefix per standard
                        print_hex_long(va_arg(args, unsigned long), 1);
                        break;
                    case 'c':
                        uart_putchar((char)va_arg(args, int));
                        break;
                    case '%':
                        uart_putchar('%');
                        break;
                    default:
                        uart_putchar('%');
                        if (alt_form) uart_putchar('#');
                        uart_putchar(*fmt);
                        break;
                }
            }
        } else {
            if (*fmt == '\n') {
                uart_putchar('\r');
            }
            uart_putchar(*fmt);
        }
        fmt++;
    }

    va_end(args);
}
