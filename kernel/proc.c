#include "proc.h"
#include "pmm.h"
#include "vmm.h"
#include "cpu.h"
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
			paddr_t pa = pmm_alloc();
			if (pa == 0) {
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

int proc_create_user(pte_t *pagetable, unsigned long entry, unsigned long ustack, unsigned long sz) {
	struct proc *p = proc_alloc();
	if (!p) {
		return -1;
	}

	p->pagetable = pagetable;
	p->sz = sz;

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

void scheduler(void) {
	for (;;) {
		// Reap dead processes
		for (int i = 0; i < NPROC; i++) {
			struct proc *p = &procs[i];
			if (p->state == UNUSED && p->kstack != 0) {
				if (p->pagetable) {
					vmm_free(p->pagetable);
					p->pagetable = 0;
				}
				pmm_free(VA_TO_PA(p->kstack));
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
			// context. If IRQ fired and timer_handler called yield(),
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

void sched(void) {
	context_switch(&current->ctx, &sched_ctx);
}

void yield(void) {
	current->state = RUNNABLE;
	sched();
}

void sleep(void *chan) {
	current->chan = chan;
	current->state = SLEEPING;
	sched();
	current->chan = 0;
}

void wakeup(void *chan) {
	for (int i = 0; i < NPROC; i++) {
		struct proc *p = &procs[i];
		if (p->state == SLEEPING && p->chan == chan) {
			p->state = RUNNABLE;
		}
	}
}

void wakeup_timed(void) {
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

void ksleep(unsigned long ticks) {
	if (ticks == 0) {
		return;
	}
	current->wakeup_tick = timer_get_ticks() + ticks;
	sleep(&current->wakeup_tick);
	current->wakeup_tick = 0;
}

void sleep_timeout(void *chan, unsigned long ticks) {
	current->chan = chan;
	if (ticks > 0) {
		current->wakeup_tick = timer_get_ticks() + ticks;
	}
	current->state = SLEEPING;
	sched();
	current->chan = 0;
	current->wakeup_tick = 0;
}
