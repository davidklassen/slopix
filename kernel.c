#include "uart.h"
#include "kprintf.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);

void kernel_main(void) {
	uart_init();

	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Hello from Slopix!\n");
}
