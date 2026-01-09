#include "uart.h"

// PL011 UART registers for QEMU virt machine
#define UART0_BASE 0x09000000

#define UART_DR     (*(volatile unsigned int *)(UART0_BASE + 0x00))
#define UART_FR     (*(volatile unsigned int *)(UART0_BASE + 0x18))
#define UART_IBRD   (*(volatile unsigned int *)(UART0_BASE + 0x24))
#define UART_FBRD   (*(volatile unsigned int *)(UART0_BASE + 0x28))
#define UART_LCRH   (*(volatile unsigned int *)(UART0_BASE + 0x2C))
#define UART_CR     (*(volatile unsigned int *)(UART0_BASE + 0x30))

// Flags
#define UART_FR_TXFF (1 << 5)  // Transmit FIFO full
#define UART_FR_BUSY (1 << 3)  // UART busy

void uart_init(void) {
    // Disable UART
    UART_CR = 0;

    // Set baud rate to 115200 (assuming 24MHz UART clock)
    // Divider = 24000000 / (16 * 115200) = 13.02
    UART_IBRD = 13;
    UART_FBRD = 1;

    // Enable FIFO, 8-bit word length
    UART_LCRH = (3 << 5) | (1 << 4);

    // Enable UART, transmit, and receive
    UART_CR = (1 << 0) | (1 << 8) | (1 << 9);
}

void uart_putchar(char c) {
    // Wait until transmit FIFO is not full
    while (UART_FR & UART_FR_TXFF);
    UART_DR = c;
}

void uart_puts(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_putchar('\r');
        }
        uart_putchar(*s++);
    }
}
