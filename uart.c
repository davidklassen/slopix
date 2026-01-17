#include "uart.h"

// PL011 UART base address on QEMU virt board
#define UART0_BASE 0x09000000

#define UART_DR_OFFSET 0x00
#define UART_FR_OFFSET 0x18
#define UART_FR_TXFF   (1 << 5) // TX FIFO full

#define UART_REG(offset) (*(volatile unsigned int *)(UART0_BASE + (offset)))

void uart_init(void) {
	// QEMU already initializes the UART, nothing to do
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
