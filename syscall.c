#include "syscall.h"
#include "exception.h"
#include "proc.h"
#include "uart.h"
#include "kprintf.h"
#include "initramfs.h"
#include "elf.h"
#include "pmem.h"
#include "arch.h"

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

static long sys_exec(const char *name) {
	char kname[32];
	int i;
	for (i = 0; i < 31 && name[i]; i++) {
		kname[i] = name[i];
	}
	kname[i] = '\0';

	struct initramfs_entry entry;
	if (initramfs_find(kname, &entry) < 0) {
		return -1;
	}

	pte_t *new_pt = uvm_create();
	if (!new_pt) {
		return -1;
	}

	unsigned long entry_addr;
	if (elf_load(entry.data, entry.size, new_pt, &entry_addr) < 0) {
		uvm_free(new_pt);
		return -1;
	}

	paddr_t stack_pa = pmem_alloc();
	if (stack_pa == 0) {
		uvm_free(new_pt);
		return -1;
	}

	if (uvm_map_page(new_pt, USER_STACK - PAGE_SIZE, stack_pa, 1, 0) < 0) {
		pmem_free(stack_pa);
		uvm_free(new_pt);
		return -1;
	}

	// Set up argc/argv on stack
	// Layout (high to low): string, argv[1]=NULL, argv[0]=&string, sp
	int namelen = 0;
	while (kname[namelen]) {
		namelen++;
	}
	int str_padded = (namelen + 1 + 7) & ~7;

	unsigned long sp = USER_STACK - str_padded - 16;
	sp &= ~0xFUL;

	// Kernel pointer to stack
	char *kstack = (char *)PA_TO_VA(stack_pa);
	unsigned long offset = sp - (USER_STACK - PAGE_SIZE);
	unsigned long *kargv = (unsigned long *)(kstack + offset);
	char *kstr = (char *)(kargv + 2);

	for (int j = 0; j <= namelen; j++) {
		kstr[j] = kname[j];
	}
	kargv[0] = sp + 16;
	kargv[1] = 0;

	// Switch to new address space
	pte_t *old_pt = current->pagetable;
	current->pagetable = new_pt;
	write_ttbr0_el1(VA_TO_PA(new_pt));
	tlbi_vmalle1();
	if (old_pt) {
		uvm_free(old_pt);
	}

	// Update trap frame for new program
	current->tf->elr = entry_addr;
	current->tf->sp_el0 = sp;
	current->tf->regs[1] = sp;

	return 1;
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
	case SYS_exec:
		ret = sys_exec((const char *)tf->regs[0]);
		break;
	default:
		kprintf("Unknown syscall %lu\n", num);
	}

	tf->regs[0] = ret;
}
