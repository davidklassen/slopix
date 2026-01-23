#include "sync.h"
#include "proc.h"
#include "kprintf.h"

void spin_lock(struct spinlock *lk) {
	disable_irq();
	if (lk->locked) {
		kprintf("panic: spin_lock %s already held\n", lk->name);
		for (;;) {
		}
	}
	lk->locked = 1;
}

void spin_unlock(struct spinlock *lk) {
	if (!lk->locked) {
		kprintf("panic: spin_unlock %s not held\n", lk->name);
		for (;;) {
		}
	}
	lk->locked = 0;
	enable_irq();
}

void sleep_lock(struct sleeplock *lk) {
	disable_irq();
	while (lk->locked) {
		enable_irq();
		proc_wait(lk);
		disable_irq();
	}
	lk->locked = 1;
	enable_irq();
}

void sleep_unlock(struct sleeplock *lk) {
	disable_irq();
	if (!lk->locked) {
		kprintf("panic: sleep_unlock %s not held\n", lk->name);
		for (;;) {
		}
	}
	lk->locked = 0;
	proc_wakeup(lk);
	enable_irq();
}
