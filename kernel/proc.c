#include "proc.h"
#include "errno.h"
#include "pmm.h"
#include "vmm.h"
#include "cpu.h"
#include "timer.h"
#include "file.h"
#include "fs.h"

struct proc procs[NPROC];
struct proc *current;
struct context sched_ctx;

static int nextpid = 1;

struct proc *proc_alloc(void) {
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state == UNUSED) {
			p->state = RUNNABLE;
			p->pid = nextpid++;
			p->pgid = p->pid;
			paddr_t pa = pmm_alloc_contiguous(KSTACK_PAGES);
			if (pa == PMM_INVALID) {
				p->state = UNUSED;
				return 0;
			}
			p->kstack = (char *)PA_TO_VA(pa);
			p->parent = 0;
			p->pagetable = 0;
			p->sz = 0;
			p->tf = 0;
			p->chan = 0;
			p->wakeup_tick = 0;
			p->pending = 0;
			p->name[0] = '\0';
			p->cwd = 0;
			return p;
		}
	}
	return 0;
}

static void proc_entry(void) {
	// New processes may start while IRQs are masked (if scheduler ran from
	// within a timer interrupt). Enable IRQs so this process can be preempted.
	// Resumed processes return via eret which restores SPSR with IRQs enabled.
	enable_irq();
	proc_func func = (proc_func)current->ctx.x19;
	func();
	current->state = UNUSED;
	proc_sched();
}

void proc_create(proc_func func) {
	struct proc *p = proc_alloc();
	if (!p) {
		return;
	}

	char *sp = p->kstack + KSTACK_SIZE;
	sp = (char *)((unsigned long)sp & ~0xFUL);

	p->ctx.x19 = (unsigned long)func;
	p->ctx.x29 = 0;
	p->ctx.x30 = (unsigned long)proc_entry;
	p->ctx.sp = (unsigned long)sp;
}

extern void usertrap_first(void);

int proc_create_user(pte_t *pagetable, unsigned long entry, unsigned long ustack, unsigned long sz) {
	struct proc *p = proc_alloc();
	if (!p) {
		return -1;
	}

	p->pagetable = pagetable;
	p->sz = sz;

	char *sp = p->kstack + KSTACK_SIZE;
	sp -= sizeof(struct trap_frame);
	sp = (char *)((unsigned long)sp & ~0xFUL);

	struct trap_frame *tf = (struct trap_frame *)sp;
	p->tf = tf;

	for (int i = 0; i < 31; i++) {
		tf->regs[i] = 0;
	}

	tf->sp_el0 = ustack;
	tf->elr = entry;
	tf->spsr = 0;

	p->ctx.x30 = (unsigned long)usertrap_first;
	p->ctx.sp = (unsigned long)tf;
	p->ctx.x29 = 0;

	return p->pid;
}

void proc_scheduler(void) {
	for (;;) {
		// Reap dead processes
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state == UNUSED && p->kstack != 0) {
				if (p->pagetable) {
					vmm_free(p->pagetable);
					p->pagetable = 0;
				}
				pmm_free_contiguous(VA_TO_PA(p->kstack), KSTACK_PAGES);
				p->kstack = 0;
			}
		}

		// Schedule runnable processes
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state != RUNNABLE) {
				continue;
			}

			// Disable IRQs while current is set but we're in scheduler
			// context. If IRQ fired and timer_handler called proc_yield(),
			// it would corrupt state by saving scheduler sp to process ctx.
			disable_irq();
			current = p;
			p->state = RUNNING;

			// Switch to process page table (if user process)
			if (p->pagetable) {
				write_ttbr0_el1(VA_TO_PA(p->pagetable));
				tlbi_vmalle1();
			}

			context_switch(&sched_ctx, &p->ctx);
			current = 0;
			enable_irq();
		}
	}
}

void proc_sched(void) {
	context_switch(&current->ctx, &sched_ctx);
}

void proc_yield(void) {
	current->state = RUNNABLE;
	proc_sched();
}

int proc_wait(void *chan) {
	if (current->pending) {
		return -EINTR;
	}
	current->chan = chan;
	current->state = SLEEPING;
	proc_sched();
	current->chan = 0;
	if (current->pending) {
		return -EINTR;
	}
	return 0;
}

void proc_wait_nointr(void *chan) {
	current->chan = chan;
	current->state = SLEEPING;
	proc_sched();
	current->chan = 0;
}

void proc_wakeup(void *chan) {
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state == SLEEPING && p->chan == chan) {
			p->state = RUNNABLE;
		}
	}
}

void proc_wakeup_timed(void) {
	unsigned long now = timer_get_ticks();
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state == SLEEPING && p->wakeup_tick != 0 &&
		    now >= p->wakeup_tick) {
			p->wakeup_tick = 0;
			p->state = RUNNABLE;
		}
	}
}

int proc_sleep(unsigned long ticks) {
	if (ticks == 0) {
		return 0;
	}
	current->wakeup_tick = timer_get_ticks() + ticks;
	int r = proc_wait(&current->wakeup_tick);
	current->wakeup_tick = 0;
	return r;
}

int proc_wait_timeout(void *chan, unsigned long ticks) {
	if (current->pending) {
		return -EINTR;
	}
	current->chan = chan;
	if (ticks > 0) {
		current->wakeup_tick = timer_get_ticks() + ticks;
	}
	current->state = SLEEPING;
	proc_sched();
	current->chan = 0;
	current->wakeup_tick = 0;
	if (current->pending) {
		return -EINTR;
	}
	return 0;
}

void proc_wait_timeout_nointr(void *chan, unsigned long ticks) {
	current->chan = chan;
	if (ticks > 0) {
		current->wakeup_tick = timer_get_ticks() + ticks;
	}
	current->state = SLEEPING;
	proc_sched();
	current->chan = 0;
	current->wakeup_tick = 0;
}

int proc_signal(int pid, int sig) {
	if (sig == 0) {
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state != UNUSED && p->pid == pid) {
				return 0;
			}
		}
		return -ESRCH;
	}

	if (sig < 1 || sig >= NSIG) {
		return -EINVAL;
	}

	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state != UNUSED && p->pid == pid) {
			p->pending |= (1 << sig);

			if ((sig == SIGCONT || sig == SIGKILL) && p->state == STOPPED) {
				p->state = RUNNABLE;
			} else if (p->state == SLEEPING) {
				p->state = RUNNABLE;
			}
			return 0;
		}
	}
	return -ESRCH;
}

int proc_setpgid(int pid, int pgid) {
	struct proc *p;

	if (pid == 0) {
		p = current;
		pid = current->pid;
	} else {
		p = 0;
		for (int i = 0; i < NPROC; i++) {
			if (procs[i].state != UNUSED && procs[i].pid == pid) {
				p = &procs[i];
				break;
			}
		}
		if (p == 0) {
			return -ESRCH;
		}
	}

	if (pgid == 0) {
		pgid = pid;
	}

	if (pgid <= 0) {
		return -EINVAL;
	}

	p->pgid = pgid;
	return 0;
}

int proc_getpgid(int pid) {
	if (pid == 0) {
		return current->pgid;
	}
	for (int i = 0; i < NPROC; i++) {
		if (procs[i].state != UNUSED && procs[i].pid == pid) {
			return procs[i].pgid;
		}
	}
	return -ESRCH;
}

int proc_signal_pgrp(int pgid, int sig) {
	if (pgid <= 0) {
		return -EINVAL;
	}
	if (sig < 1 || sig >= NSIG) {
		return -EINVAL;
	}

	int found = 0;
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state != UNUSED && p->pgid == pgid) {
			p->pending |= (1 << sig);
			if ((sig == SIGCONT || sig == SIGKILL) && p->state == STOPPED) {
				p->state = RUNNABLE;
			} else if (p->state == SLEEPING) {
				p->state = RUNNABLE;
			}
			found = 1;
		}
	}
	return found ? 0 : -ESRCH;
}

void proc_check_signals(void) {
	if (current->pending == 0) {
		return;
	}

	if (current->pending & (1 << SIGKILL)) {
		current->pending &= ~(1 << SIGKILL);
		current->exit_status = -SIGKILL;
		goto do_exit;
	}

	int stop_sigs[] = {SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU};
	for (int i = 0; i < 4; i++) {
		if (current->pending & (1 << stop_sigs[i])) {
			current->pending &= ~(1 << stop_sigs[i]);
			current->state = STOPPED;
			current->stop_signal = stop_sigs[i];
			if (current->parent) {
				proc_wakeup(current->parent);
			}
			proc_sched();
			current->pending &= ~(1 << SIGCONT);
			return;
		}
	}

	if (current->pending & (1 << SIGCONT)) {
		current->pending &= ~(1 << SIGCONT);
	}

	int term_sigs[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGPIPE, SIGALRM, SIGUSR1, SIGUSR2};
	for (int i = 0; i < 8; i++) {
		if (current->pending & (1 << term_sigs[i])) {
			current->pending &= ~(1 << term_sigs[i]);
			current->exit_status = -term_sigs[i];
			goto do_exit;
		}
	}

	return;

do_exit:
	for (int fd = 0; fd < NOFILE; fd++) {
		if (current->ofile[fd]) {
			fileclose(current->ofile[fd]);
			current->ofile[fd] = 0;
		}
	}
	if (current->cwd) {
		fs_iput(current->cwd);
		current->cwd = 0;
	}
	if (current->parent) {
		current->state = ZOMBIE;
		proc_wakeup(current->parent);
	} else {
		current->state = UNUSED;
	}
	proc_sched();
}
