#ifndef PROC_H
#define PROC_H

#include "mmu.h"
#include "exception.h"

struct context {
	unsigned long x19, x20, x21, x22, x23;
	unsigned long x24, x25, x26, x27, x28;
	unsigned long x29;
	unsigned long x30;
	unsigned long sp;
};

enum proc_state { UNUSED,
		  RUNNABLE,
		  RUNNING };

struct proc {
	enum proc_state state;
	int pid;
	char *kstack;
	struct context ctx;

	// User mode support
	pte_t *pagetable;
	unsigned long sz;
	struct trap_frame *tf;
};

#define NPROC 8

extern struct proc procs[NPROC];
extern struct proc *current;
extern struct context sched_ctx;

typedef void (*proc_func)(void);

struct proc *proc_alloc(void);
void proc_create(proc_func func);
void context_switch(struct context *prev, struct context *next);
void scheduler(void);
void sched(void);
void yield(void);
void ksleep(unsigned long ticks);

#endif
