// uart.c - Minimal PL011 UART driver

#define KERNEL_BASE 0xFFFF000000000000UL
#define UART_PA     0x09000000UL
#define UART_BASE   (KERNEL_BASE + UART_PA)

// PL011 registers (offsets from base)
#define UART_DR     (*(volatile unsigned int *)(UART_BASE + 0x00))
#define UART_FR     (*(volatile unsigned int *)(UART_BASE + 0x18))
#define UART_CR     (*(volatile unsigned int *)(UART_BASE + 0x30))

// Flag register bits
#define FR_TXFF     (1 << 5)  // TX FIFO full

void uart_init(void) {
    // Enable TX
    UART_CR = (1 << 8) | (1 << 0);  // TXE | UARTEN
}

void uart_putc(char c) {
    // Wait until TX FIFO has space
    while (UART_FR & FR_TXFF)
        ;
    UART_DR = c;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

void uart_puthex(unsigned long v) {
    static const char hex[] = "0123456789abcdef";
    uart_puts("0x");
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(hex[(v >> i) & 0xf]);
    }
}
