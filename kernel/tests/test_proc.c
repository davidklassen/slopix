#ifdef RUN_TESTS

#include "test.h"
#include "proc.h"
#include "pmm.h"

TEST(proc_alloc_returns_embryo) {
	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "proc_alloc should succeed");
	ASSERT_EQ(EMBRYO, p->state, "New process should be EMBRYO, not RUNNABLE");

	// Clean up: free kstack and release slot
	pmm_free_contiguous(VA_TO_PA(p->kstack), KSTACK_PAGES);
	p->kstack = 0;
	p->state = UNUSED;
	return 0;
}

TEST(proc_alloc_zeroes_ofile) {
	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "proc_alloc should succeed");
	for (int i = 0; i < NOFILE; i++) {
		ASSERT_NULL(p->ofile[i], "ofile should be NULL");
	}

	pmm_free_contiguous(VA_TO_PA(p->kstack), KSTACK_PAGES);
	p->kstack = 0;
	p->state = UNUSED;
	return 0;
}

TEST(proc_alloc_zeroes_cwd) {
	struct proc *p = proc_alloc();
	ASSERT_NOT_NULL(p, "proc_alloc should succeed");
	ASSERT_NULL(p->cwd, "cwd should be NULL");

	pmm_free_contiguous(VA_TO_PA(p->kstack), KSTACK_PAGES);
	p->kstack = 0;
	p->state = UNUSED;
	return 0;
}

TEST_SUITE(proc) {
	RUN_TEST(proc_alloc_returns_embryo);
	RUN_TEST(proc_alloc_zeroes_ofile);
	RUN_TEST(proc_alloc_zeroes_cwd);
}

#endif
