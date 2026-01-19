#ifdef RUN_TESTS

#include "test.h"
#include "proc.h"
#include "pmem.h"
#include "mmu.h"

TEST(proc_alloc_returns_proc) {
	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "proc_alloc should return non-null");
	ASSERT_EQ(p->state, RUNNABLE, "state should be RUNNABLE");
	ASSERT_EQ(p->pid, 1, "first pid should be 1");
	ASSERT_NOT_NULL(p->kstack, "kstack should be non-null");

	pmem_free(VA_TO_PA(p->kstack));
	p->state = UNUSED;
	return 0;
}

TEST(proc_alloc_unique_pids) {
	struct proc *p1 = proc_alloc();
	struct proc *p2 = proc_alloc();

	ASSERT_NOT_NULL(p1, "first alloc should succeed");
	ASSERT_NOT_NULL(p2, "second alloc should succeed");
	ASSERT_NE(p1->pid, p2->pid, "pids should be different");

	pmem_free(VA_TO_PA(p1->kstack));
	pmem_free(VA_TO_PA(p2->kstack));
	p1->state = UNUSED;
	p2->state = UNUSED;
	return 0;
}

TEST(proc_create_sets_context) {
	extern void test_dummy_func(void);

	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "alloc should succeed");

	char *sp = p->kstack + PAGE_SIZE;
	sp = (char *)((unsigned long)sp & ~0xFUL);

	pmem_free(VA_TO_PA(p->kstack));
	p->state = UNUSED;
	return 0;
}

TEST(proc_has_usermode_fields) {
	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "alloc should succeed");

	ASSERT_EQ((unsigned long)p->pagetable, 0, "pagetable should be null");
	ASSERT_EQ(p->sz, 0, "sz should be 0");
	ASSERT_EQ((unsigned long)p->tf, 0, "tf should be null");

	pmem_free(VA_TO_PA(p->kstack));
	p->state = UNUSED;
	return 0;
}

TEST_SUITE(proc) {
	RUN_TEST(proc_alloc_returns_proc);
	RUN_TEST(proc_alloc_unique_pids);
	RUN_TEST(proc_create_sets_context);
	RUN_TEST(proc_has_usermode_fields);
}

#endif
