#ifndef PROC_H
#define PROC_H

#include "vmm.h"
#include "exception.h"
#include "signal.h"

struct inode;
struct file;

#define NOFILE	     16
#define KSTACK_PAGES 4
#define KSTACK_SIZE  (KSTACK_PAGES * PAGE_SIZE)

struct context {
	unsigned long x19, x20, x21, x22, x23;
	unsigned long x24, x25, x26, x27, x28;
	unsigned long x29;
	unsigned long x30;
	unsigned long sp;
};

enum proc_state { UNUSED,
		  RUNNABLE,
		  RUNNING,
		  SLEEPING,
		  STOPPED,
		  ZOMBIE };

struct proc {
	enum proc_state state;
	int pid;
	int pgid;
	struct proc *parent;
	char *kstack;
	struct context ctx;

	// User mode support
	pte_t *pagetable;
	unsigned long sz;
	struct trap_frame *tf;

	// Sleep channel support
	void *chan;
	unsigned long wakeup_tick;

	int exit_status;
	int stop_signal;

	unsigned int pending;
	char name[16];

	// Filesystem support
	struct inode *cwd;
	struct file *ofile[NOFILE];
};

#define NPROC 64

extern struct proc procs[NPROC];
extern struct proc *current;
extern struct context sched_ctx;

typedef void (*proc_func)(void);

struct proc *proc_alloc(void);
void proc_create(proc_func func);
int proc_create_user(pte_t *pagetable, unsigned long entry, unsigned long ustack, unsigned long sz);
void context_switch(struct context *prev, struct context *next);
void proc_scheduler(void);
void proc_sched(void);
void proc_yield(void);
void proc_sleep(unsigned long ticks);
void proc_wait(void *chan);
void proc_wait_timeout(void *chan, unsigned long ticks);
void proc_wakeup(void *chan);
void proc_wakeup_timed(void);
#define proc_is_killed(p) ((p)->pending & (1 << SIGKILL))

int proc_signal(int pid, int sig);
int proc_setpgid(int pid, int pgid);
int proc_getpgid(int pid);
int proc_signal_pgrp(int pgid, int sig);
void proc_check_signals(void);

#endif
