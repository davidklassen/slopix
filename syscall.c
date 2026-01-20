#include "syscall.h"
#include "exception.h"
#include "proc.h"
#include "uart.h"
#include "kprintf.h"
#include "initramfs.h"
#include "elf.h"
#include "pmm.h"
#include "cpu.h"
#include "vmm.h"

static long sys_write(int fd, const char *buf, unsigned long len) {
	if (fd != 1) {
		return -1;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 0) < 0) {
		return -1;
	}

	for (unsigned long i = 0; i < len; i++) {
		uart_putc(buf[i]);
	}
	return len;
}

static long sys_exit(int status) {
	(void)status;
	if (current->parent) {
		current->state = ZOMBIE;
		wakeup(current->parent);
	} else {
		current->state = UNUSED;
	}
	sched();
	return 0;
}

static long sys_read(int fd, char *buf, unsigned long len) {
	if (fd != 0) {
		return -1;
	}

	if (vmm_validate(current->pagetable, (unsigned long)buf, len, 1) < 0) {
		return -1;
	}

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

static long sys_sleep(unsigned long ms) {
	unsigned long ticks = ms / 10;
	if (ticks == 0 && ms > 0) {
		ticks = 1;
	}
	ksleep(ticks);
	return 0;
}

static long sys_getpid(void) {
	return current->pid;
}

static long sys_exec(const char *cmdline) {
	// Safely copy command line from user space
	char kcmd[128];
	if (vmm_copyinstr(current->pagetable, kcmd, (unsigned long)cmdline, 128) < 0) {
		return -1;
	}

	// Parse into argv (max 16 args)
	char *argv[16];
	int argc = 0;
	char *p = kcmd;

	while (*p && argc < 16) {
		while (*p == ' ') {
			p++;
		}
		if (*p == '\0') {
			break;
		}
		argv[argc++] = p;
		while (*p && *p != ' ') {
			p++;
		}
		if (*p) {
			*p++ = '\0';
		}
	}

	if (argc == 0) {
		return -1;
	}

	// First arg is program name
	struct initramfs_entry entry;
	if (initramfs_find(argv[0], &entry) < 0) {
		return -1;
	}

	pte_t *new_pt = vmm_create();
	if (!new_pt) {
		return -1;
	}

	unsigned long entry_addr;
	if (elf_load(entry.data, entry.size, new_pt, &entry_addr) < 0) {
		vmm_free(new_pt);
		return -1;
	}

	paddr_t stack_pa = pmm_alloc();
	if (stack_pa == 0) {
		vmm_free(new_pt);
		return -1;
	}

	if (vmm_map_page(new_pt, USER_STACK - PAGE_SIZE, stack_pa, 1, 0) < 0) {
		pmm_free(stack_pa);
		vmm_free(new_pt);
		return -1;
	}

	// Set up argc/argv on stack
	// Layout (high to low): strings, argv[argc]=NULL, argv[0..argc-1], sp
	char *kstack_top = (char *)PA_TO_VA(stack_pa) + PAGE_SIZE;
	unsigned long ustack_top = USER_STACK;

	// Copy strings to stack (from top down)
	unsigned long ustr[16];
	for (int j = argc - 1; j >= 0; j--) {
		int len = 0;
		while (argv[j][len]) {
			len++;
		}
		len++;
		kstack_top -= len;
		ustack_top -= len;
		for (int k = 0; k < len; k++) {
			kstack_top[k] = argv[j][k];
		}
		ustr[j] = ustack_top;
	}

	// Align to 8 bytes
	ustack_top &= ~7UL;
	kstack_top = (char *)PA_TO_VA(stack_pa) + PAGE_SIZE - (USER_STACK - ustack_top);

	// argv array: argv[0..argc-1], NULL
	int argv_size = (argc + 1) * 8;
	ustack_top -= argv_size;
	kstack_top -= argv_size;
	unsigned long *kargv = (unsigned long *)kstack_top;
	for (int j = 0; j < argc; j++) {
		kargv[j] = ustr[j];
	}
	kargv[argc] = 0;

	// 16-byte align sp
	unsigned long sp = ustack_top & ~0xFUL;

	// Switch to new address space
	pte_t *old_pt = current->pagetable;
	current->pagetable = new_pt;
	write_ttbr0_el1(VA_TO_PA(new_pt));
	tlbi_vmalle1();
	if (old_pt) {
		vmm_free(old_pt);
	}

	// Update trap frame for new program
	current->tf->elr = entry_addr;
	current->tf->sp_el0 = sp;
	current->tf->regs[1] = ustack_top;

	return argc;
}

static long sys_fork(void) {
	pte_t *child_pt = vmm_copy(current->pagetable);
	if (child_pt == 0) {
		return -1;
	}

	struct proc *child = proc_alloc();
	if (child == 0) {
		vmm_free(child_pt);
		return -1;
	}

	child->pagetable = child_pt;
	child->parent = current;

	// Copy trap frame to child's kernel stack
	char *sp = child->kstack + PAGE_SIZE;
	sp -= sizeof(struct trap_frame);
	sp = (char *)((unsigned long)sp & ~0xFUL);

	struct trap_frame *child_tf = (struct trap_frame *)sp;
	child->tf = child_tf;

	// Copy parent's trap frame
	for (int i = 0; i < 31; i++) {
		child_tf->regs[i] = current->tf->regs[i];
	}
	child_tf->sp_el0 = current->tf->sp_el0;
	child_tf->elr = current->tf->elr;
	child_tf->spsr = current->tf->spsr;

	// Child returns 0 from fork
	child_tf->regs[0] = 0;

	// Set up child context to return to userspace
	extern void usertrap_first(void);
	child->ctx.x30 = (unsigned long)usertrap_first;
	child->ctx.sp = (unsigned long)child_tf;
	child->ctx.x29 = 0;

	// Parent returns child's pid
	return child->pid;
}

static long sys_wait(void) {
	for (;;) {
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state == ZOMBIE && p->parent == current) {
				int pid = p->pid;
				p->state = UNUSED;
				return pid;
			}
		}
		sleep(current);
	}
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
	case SYS_fork:
		ret = sys_fork();
		break;
	case SYS_wait:
		ret = sys_wait();
		break;
	default:
		kprintf("Unknown syscall %lu\n", num);
	}

	tf->regs[0] = ret;
}
