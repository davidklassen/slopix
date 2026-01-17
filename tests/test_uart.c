#include "test.h"
#include "uart.h"

#ifdef RUN_TESTS

TEST(putc_works) {
	uart_putc('X');
	return 0;
}

TEST(puts_works) {
	uart_puts("test");
	return 0;
}

#endif

TEST_SUITE(uart) {
	RUN_TEST(putc_works);
	RUN_TEST(puts_works);
}
