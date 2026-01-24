#include "console.h"
#include "file.h"
#include "proc.h"
#include "signal.h"
#include "uart.h"

struct devsw devsw[NDEV];

static int fg_pgid = 0;

void console_set_fg_pgid(int pgid) {
	if (pgid >= 0) {
		fg_pgid = pgid;
	}
}

int console_get_fg_pgid(void) {
	return fg_pgid;
}

int console_read(char *dst, int n) {
	extern struct proc *current;
	if (fg_pgid > 0 && current->pgid != fg_pgid) {
		proc_signal_pgrp(current->pgid, SIGTTIN);
		return -1;
	}
	return uart_read(dst, n);
}

int console_write(const char *src, int n) {
	for (int i = 0; i < n; i++) {
		uart_putc(src[i]);
	}
	return n;
}

static int nullread(char *dst, int n) {
	(void)dst;
	(void)n;
	return 0;
}

static int nullwrite(const char *src, int n) {
	(void)src;
	return n;
}

void console_init(void) {
	devsw[CONSOLE].read = console_read;
	devsw[CONSOLE].write = console_write;
	devsw[NULLDEV].read = nullread;
	devsw[NULLDEV].write = nullwrite;
}
