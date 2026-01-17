#include "uart.h"
#include "kprintf.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);

static void echo_loop(void) {
	uart_puts("slopix> ");
	for (;;) {
		char c = uart_getc();
		if (c == '\r' || c == '\n') {
			uart_puts("\nslopix> ");
		} else {
			uart_putc(c);
		}
	}
}

void kernel_main(void) {
	uart_init();

	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Hello from Slopix!\n");
	echo_loop();
}
