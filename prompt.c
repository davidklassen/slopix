#include "prompt.h"
#include "uart.h"
#include "timer.h"
#include "arch.h"

#define BLINK_INTERVAL 50

void prompt(void) {
	uart_puts("slopix> ");
	uart_puts("\x1b[?25h");

	unsigned long last_blink = timer_get_ticks();
	int cursor_visible = 1;

	for (;;) {
		int c = uart_getc_nb();

		if (c >= 0) {
			if (!cursor_visible) {
				uart_puts("\x1b[?25h");
				cursor_visible = 1;
			}
			last_blink = timer_get_ticks();

			if (c == '\r' || c == '\n') {
				uart_puts("\nslopix> ");
			} else {
				uart_putc((char)c);
			}
		}

		unsigned long now = timer_get_ticks();
		if (now - last_blink >= BLINK_INTERVAL) {
			last_blink = now;
			if (cursor_visible) {
				uart_puts("\x1b[?25l");
				cursor_visible = 0;
			} else {
				uart_puts("\x1b[?25h");
				cursor_visible = 1;
			}
		}

		wfi();
	}
}
