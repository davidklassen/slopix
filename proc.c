#include "proc.h"
#include "pmem.h"
#include "mmu.h"
#include "arch.h"
#include "timer.h"

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
			paddr_t pa = pmem_alloc();
			if (pa == 0) {
				p->state = UNUSED;
				return 0;
			}
			p->kstack = (char *)PA_TO_VA(pa);
			p->parent = 0;
			p->pagetable = 0;
			p->sz = 0;
			p->tf = 0;
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
	sched();
}

void proc_create(proc_func func) {
	struct proc *p = proc_alloc();
	if (!p) {
		return;
	}

	char *sp = p->kstack + PAGE_SIZE;
	sp = (char *)((unsigned long)sp & ~0xFUL);

	p->ctx.x19 = (unsigned long)func;
	p->ctx.x29 = 0;
	p->ctx.x30 = (unsigned long)proc_entry;
	p->ctx.sp = (unsigned long)sp;
}

extern void usertrap_return(paddr_t pagetable_pa);

void usertrap_first(void) {
	enable_irq();
	usertrap_return(VA_TO_PA(current->pagetable));
}

int proc_create_user(pte_t *pagetable, unsigned long entry, unsigned long ustack) {
	struct proc *p = proc_alloc();
	if (!p) {
		return -1;
	}

	p->pagetable = pagetable;

	char *sp = p->kstack + PAGE_SIZE;
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

void proc_free(struct proc *p) {
	if (p->pagetable) {
		uvm_free(p->pagetable);
	}
	pmem_free(VA_TO_PA(p->kstack));
	p->state = UNUSED;
}

void scheduler(void) {
	for (;;) {
		// Reap dead processes
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state == UNUSED && p->kstack != 0) {
				if (p->pagetable) {
					uvm_free(p->pagetable);
					p->pagetable = 0;
				}
				pmem_free(VA_TO_PA(p->kstack));
				p->kstack = 0;
			}
		}

		// Schedule runnable processes
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state != RUNNABLE) {
				continue;
			}
			current = p;
			p->state = RUNNING;

			// Switch to process page table (if user process)
			if (p->pagetable) {
				write_ttbr0_el1(VA_TO_PA(p->pagetable));
				tlbi_vmalle1();
			}

			context_switch(&sched_ctx, &p->ctx);
			current = 0;
		}
	}
}

void sched(void) {
	context_switch(&current->ctx, &sched_ctx);
}

void yield(void) {
	current->state = RUNNABLE;
	sched();
}

void ksleep(unsigned long ticks) {
	unsigned long target = timer_get_ticks() + ticks;
	while (timer_get_ticks() < target) {
		yield();
	}
}
