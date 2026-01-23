#ifndef SYNC_H
#define SYNC_H

#include "cpu.h"

// Spinlock: for short critical sections
// On single-CPU, this just disables interrupts
struct spinlock {
	int locked;
	const char *name;
};

#define SPINLOCK_INIT(n) {.locked = 0, .name = (n)}

static inline void spin_init(struct spinlock *lk, const char *name) {
	lk->locked = 0;
	lk->name = name;
}

static inline int spin_holding(struct spinlock *lk) {
	return lk->locked;
}

void spin_lock(struct spinlock *lk);
void spin_unlock(struct spinlock *lk);

// Sleeplock: for longer critical sections (I/O, filesystem)
// Allows other processes to run while waiting
struct sleeplock {
	int locked;
	const char *name;
};

#define SLEEPLOCK_INIT(n) {.locked = 0, .name = (n)}

static inline void sleep_init(struct sleeplock *lk, const char *name) {
	lk->locked = 0;
	lk->name = name;
}

static inline int sleep_holding(struct sleeplock *lk) {
	return lk->locked;
}

void sleep_lock(struct sleeplock *lk);
void sleep_unlock(struct sleeplock *lk);

#endif
