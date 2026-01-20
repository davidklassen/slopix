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

// Interrupt registers
#define UART_IMSC_OFFSET 0x38
#define UART_ICR_OFFSET	 0x44
#define UART_IMSC_RXIM	 (1 << 4)

// UART0 interrupt (SPI 1 = INTID 33)
#define UART_IRQ 33

void uart_init(void);
void uart_init_irq(void);
void uart_irq_handler(void);
int uart_read(char *buf, unsigned long len);
int uart_poll(void);
int uart_poll_timeout(unsigned long ticks);
void uart_putc(char c);
void uart_puts(const char *s);
char uart_getc(void);
int uart_getc_nb(void);

#endif
