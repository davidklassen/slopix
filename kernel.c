#include "uart.h"
#include "gic.h"
#include "timer.h"
#include "prompt.h"
#include "arch.h"
#include "tests/test.h"

DECLARE_SUITE(uart);
DECLARE_SUITE(kprintf);
DECLARE_SUITE(exception);
DECLARE_SUITE(timer);

void kernel_main(void) {
	uart_init();
	RUN_SUITE(uart);
	RUN_SUITE(kprintf);
	RUN_SUITE(exception);

	gic_init();
	timer_init();
	enable_irq();
	RUN_SUITE(timer);

	TEST_REPORT();
	TEST_EXIT();

	uart_puts("Welcome to Slopix!\n");
	uart_puts("To exit, press Ctrl-a x\n\n");

	prompt();
}
