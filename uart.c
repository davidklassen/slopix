#include "uart.h"

void uart_init(void) {
	UART_REG(UART_CR_OFFSET) = 0;

	while (UART_REG(UART_FR_OFFSET) & UART_FR_BUSY)
		;

	UART_REG(UART_LCR_H_OFFSET) = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
	UART_REG(UART_CR_OFFSET) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_putc(char c) {
	// Wait until TX FIFO is not full
	while (UART_REG(UART_FR_OFFSET) & UART_FR_TXFF)
		;
	UART_REG(UART_DR_OFFSET) = c;
}

void uart_puts(const char *s) {
	while (*s) {
		if (*s == '\n') {
			uart_putc('\r');
		}
		uart_putc(*s++);
	}
}

int uart_getc_nb(void) {
	if (UART_REG(UART_FR_OFFSET) & UART_FR_RXFE) {
		return -1;
	}
	return UART_REG(UART_DR_OFFSET) & 0xFF;
}

char uart_getc(void) {
	while (UART_REG(UART_FR_OFFSET) & UART_FR_RXFE)
		;
	return UART_REG(UART_DR_OFFSET) & 0xFF;
}
