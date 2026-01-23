#include "console.h"
#include "file.h"
#include "uart.h"

struct devsw devsw[NDEV];

int console_read(char *dst, int n) {
	return uart_read(dst, n);
}

int console_write(const char *src, int n) {
	for (int i = 0; i < n; i++) {
		uart_putc(src[i]);
	}
	return n;
}

static int nullread(char *dst, int n) {
	(void)dst;
	(void)n;
	return 0;
}

static int nullwrite(const char *src, int n) {
	(void)src;
	return n;
}

void console_init(void) {
	devsw[CONSOLE].read = console_read;
	devsw[CONSOLE].write = console_write;
	devsw[NULLDEV].read = nullread;
	devsw[NULLDEV].write = nullwrite;
}
