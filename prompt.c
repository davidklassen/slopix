#include "prompt.h"
#include "uart.h"

void prompt(void) {
	uart_puts("slopix> ");

	for (;;) {
		int c = uart_getc_nb();
		if (c >= 0) {
			if (c == '\r' || c == '\n') {
				uart_puts("\nslopix> ");
			} else {
				uart_putc((char)c);
			}
		}
	}
}
