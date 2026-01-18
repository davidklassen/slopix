#include "uart.h"
#include "kprintf.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);

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
	RUN_SUITE(exception);
	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Welcome to Slopix!\n");
	uart_puts("To exit, press Ctrl-a x\n\n");
	echo_loop();
}
