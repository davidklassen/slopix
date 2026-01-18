#include "uart.h"

static volatile unsigned int *uart_base = (volatile unsigned int *)UART0_PHYS;

#define UART_BASE_REG(offset) (*(uart_base + ((offset) / sizeof(unsigned int))))

void uart_init(void) {
	UART_BASE_REG(UART_CR_OFFSET) = 0;

	while (UART_BASE_REG(UART_FR_OFFSET) & UART_FR_BUSY)
		;

	UART_BASE_REG(UART_LCR_H_OFFSET) = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
	UART_BASE_REG(UART_CR_OFFSET) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_putc(char c) {
	while (UART_BASE_REG(UART_FR_OFFSET) & UART_FR_TXFF)
		;
	UART_BASE_REG(UART_DR_OFFSET) = c;
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
	if (UART_BASE_REG(UART_FR_OFFSET) & UART_FR_RXFE) {
		return -1;
	}
	return UART_BASE_REG(UART_DR_OFFSET) & 0xFF;
}

char uart_getc(void) {
	while (UART_BASE_REG(UART_FR_OFFSET) & UART_FR_RXFE)
		;
	return UART_BASE_REG(UART_DR_OFFSET) & 0xFF;
}

void uart_use_virtual_address(void) {
	uart_base = (volatile unsigned int *)UART0_VIRT;
}
