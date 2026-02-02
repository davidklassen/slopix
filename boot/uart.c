#define UART0_PA 0x09000000UL

#define UART_REG(off) (*(volatile unsigned int *)(UART0_PA + (off)))

#define UART_DR_OFFSET	  0x00
#define UART_FR_OFFSET	  0x18
#define UART_LCR_H_OFFSET 0x2C
#define UART_CR_OFFSET	  0x30

#define UART_FR_TXFF (1 << 5)
#define UART_FR_BUSY (1 << 3)

#define UART_CR_UARTEN (1 << 0)
#define UART_CR_TXE    (1 << 8)

#define UART_LCR_H_FEN	 (1 << 4)
#define UART_LCR_H_WLEN8 (3 << 5)

void uart_init(void) {
	UART_REG(UART_CR_OFFSET) = 0;
	while (UART_REG(UART_FR_OFFSET) & UART_FR_BUSY)
		;
	UART_REG(UART_LCR_H_OFFSET) = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
	UART_REG(UART_CR_OFFSET) = UART_CR_UARTEN | UART_CR_TXE;
}

void uart_putc(char c) {
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

void uart_puthex(unsigned int val) {
	static const char hex[] = "0123456789abcdef";
	for (int i = 28; i >= 0; i -= 4) {
		uart_putc(hex[(val >> i) & 0xF]);
	}
}

void uart_puthex8(unsigned char val) {
	static const char hex[] = "0123456789abcdef";
	uart_putc(hex[(val >> 4) & 0xF]);
	uart_putc(hex[val & 0xF]);
}
