#include "prompt.h"
#include "uart.h"
#include "proc.h"

void cursor_blink(void) {
	for (;;) {
		uart_puts("\x1b[?25h");
		ksleep(50);
		uart_puts("\x1b[?25l");
		ksleep(50);
	}
}

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
