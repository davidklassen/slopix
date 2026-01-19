#include "syscall.h"
#include "exception.h"
#include "proc.h"
#include "uart.h"
#include "kprintf.h"

static long sys_write(int fd, const char *buf, unsigned long len) {
	if (fd != 1) {
		return -1;
	}

	// TODO: validate user pointer
	for (unsigned long i = 0; i < len; i++) {
		uart_putc(buf[i]);
	}
	return len;
}

static long sys_exit(int status) {
	kprintf("Process %d exited with status %d\n", current->pid, status);
	current->state = UNUSED;
	sched();
	return 0;
}

static long sys_read(int fd, char *buf, unsigned long len) {
	if (fd != 0) {
		return -1;
	}

	// TODO: validate user pointer
	unsigned long i = 0;
	while (i < len) {
		int c = uart_getc_nb();
		if (c < 0) {
			break;
		}
		buf[i++] = c;
	}
	return i;
}

static long sys_sleep(unsigned long ticks) {
	ksleep(ticks);
	return 0;
}

static long sys_getpid(void) {
	return current->pid;
}

void syscall(struct trap_frame *tf) {
	long ret = -1;
	unsigned long num = tf->regs[8];

	switch (num) {
	case SYS_write:
		ret = sys_write(tf->regs[0], (const char *)tf->regs[1], tf->regs[2]);
		break;
	case SYS_exit:
		ret = sys_exit(tf->regs[0]);
		break;
	case SYS_read:
		ret = sys_read(tf->regs[0], (char *)tf->regs[1], tf->regs[2]);
		break;
	case SYS_sleep:
		ret = sys_sleep(tf->regs[0]);
		break;
	case SYS_getpid:
		ret = sys_getpid();
		break;
	default:
		kprintf("Unknown syscall %lu\n", num);
	}

	tf->regs[0] = ret;
}
