#ifdef RUN_TESTS

#include "test.h"
#include "uart.h"

#define UART_REG(off) (*(volatile unsigned int *)(UART0_PHYS + (off)))

TEST(putc_works) {
	uart_putc('X');
	return 0;
}

TEST(puts_works) {
	uart_puts("test");
	return 0;
}

TEST(init_enables_rx) {
	unsigned int expected = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
	ASSERT((UART_REG(UART_CR_OFFSET) & expected) == expected,
	       "UART CR not set correctly");
	return 0;
}

TEST(getc_nb_empty) {
	int c = uart_getc_nb();
	ASSERT_EQ(c, -1, "Should return -1 when FIFO empty");
	return 0;
}

TEST_SUITE(uart) {
	RUN_TEST(putc_works);
	RUN_TEST(puts_works);
	RUN_TEST(init_enables_rx);
	RUN_TEST(getc_nb_empty);
}

#endif
