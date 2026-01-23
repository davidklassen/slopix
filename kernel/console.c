#include "console.h"
#include "file.h"
#include "uart.h"

struct devsw devsw[NDEV];

int consoleread(char *dst, int n) {
	return uart_read(dst, n);
}

int consolewrite(const char *src, int n) {
	for (int i = 0; i < n; i++) {
		uart_putc(src[i]);
	}
	return n;
}

void consoleinit(void) {
	devsw[CONSOLE].read = consoleread;
	devsw[CONSOLE].write = consolewrite;
}
