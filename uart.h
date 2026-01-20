#ifndef UART_H
#define UART_H

#include "board.h"

// Register access macro
#define UART_REG(off) (*(volatile unsigned int *)(UART0_VA + (off)))

// Register offsets
#define UART_DR_OFFSET	  0x00
#define UART_FR_OFFSET	  0x18
#define UART_LCR_H_OFFSET 0x2C
#define UART_CR_OFFSET	  0x30

#define UART_FR_TXFF (1 << 5)
#define UART_FR_RXFE (1 << 4)
#define UART_FR_BUSY (1 << 3)

#define UART_CR_UARTEN (1 << 0)
#define UART_CR_TXE    (1 << 8)
#define UART_CR_RXE    (1 << 9)

#define UART_LCR_H_FEN	 (1 << 4)
#define UART_LCR_H_WLEN8 (3 << 5)

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);
char uart_getc(void);
int uart_getc_nb(void);

#endif
