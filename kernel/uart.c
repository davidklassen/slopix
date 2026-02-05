#include "uart.h"
#include "errno.h"
#include "kprintf.h"
#include "proc.h"
#include "gic.h"
#include "cpu.h"
#include "console.h"
#include "signal.h"

#define UART_RX_BUF_SIZE 64

static struct {
	char buf[UART_RX_BUF_SIZE];
	unsigned int head;
	unsigned int tail;
} uart_rx;

void uart_init(void) {
	UART_REG(UART_CR_OFFSET) = 0;

	while (UART_REG(UART_FR_OFFSET) & UART_FR_BUSY)
		;

	UART_REG(UART_LCR_H_OFFSET) = UART_LCR_H_WLEN8 | UART_LCR_H_FEN;
	UART_REG(UART_CR_OFFSET) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
	isb();
	kprintf("uart: initialized\n");
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

void uart_init_irq(void) {
	uart_rx.head = 0;
	uart_rx.tail = 0;
	UART_REG(UART_IMSC_OFFSET) = UART_IMSC_RXIM;
	gic_enable_irq(UART_IRQ);
}

void uart_irq_handler(void) {
	int got_data = 0;
	while (!(UART_REG(UART_FR_OFFSET) & UART_FR_RXFE)) {
		char c = UART_REG(UART_DR_OFFSET) & 0xFF;

		if (!console_get_raw()) {
			if (c == 0x03) {
				int pgid = console_get_fg_pgid();
				if (pgid > 0) {
					proc_signal_pgrp(pgid, SIGINT);
				}
				continue;
			}
			if (c == 0x1A) {
				int pgid = console_get_fg_pgid();
				if (pgid > 0) {
					proc_signal_pgrp(pgid, SIGTSTP);
				}
				continue;
			}
		}

		unsigned int next = (uart_rx.head + 1) % UART_RX_BUF_SIZE;
		if (next != uart_rx.tail) {
			uart_rx.buf[uart_rx.head] = c;
			uart_rx.head = next;
			got_data = 1;
		}
	}
	UART_REG(UART_ICR_OFFSET) = UART_IMSC_RXIM;
	if (got_data) {
		proc_wakeup(&uart_rx);
	}
}

int uart_read(char *buf, unsigned long len) {
	unsigned long i = 0;
	while (i < len) {
		while (uart_rx.head == uart_rx.tail) {
			if (proc_wait(&uart_rx) < 0) {
				return i > 0 ? (int)i : -EINTR;
			}
		}
		buf[i++] = uart_rx.buf[uart_rx.tail];
		uart_rx.tail = (uart_rx.tail + 1) % UART_RX_BUF_SIZE;
		break;
	}
	return i;
}

int uart_poll(void) {
	return uart_rx.head != uart_rx.tail;
}

int uart_poll_timeout(unsigned long ticks) {
	if (uart_poll()) {
		return 1;
	}
	if (ticks == 0) {
		return 0;
	}
	if (proc_wait_timeout(&uart_rx, ticks) < 0) {
		return -EINTR;
	}
	return uart_poll();
}
