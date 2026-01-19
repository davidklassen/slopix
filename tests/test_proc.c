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

TEST(proc_free_releases_all_memory) {
	unsigned long before = pmem_free_count();

	// Create user page table with mappings
	pte_t *pt = uvm_create();
	ASSERT_NOT_NULL(pt, "uvm_create should succeed");
	paddr_t page = pmem_alloc();
	ASSERT(page != 0, "pmem_alloc should succeed");
	int ret = uvm_map_page(pt, 0x1000, page, 1, 0);
	ASSERT_EQ(ret, 0, "uvm_map_page should succeed");

	// Create user process
	int pid = proc_create_user(pt, 0x1000, 0x2000);
	ASSERT(pid > 0, "proc_create_user should succeed");

	// Find the process
	struct proc *p = 0;
	for (int i = 0; i < NPROC; i++) {
		if (procs[i].pid == pid) {
			p = &procs[i];
			break;
		}
	}
	ASSERT_NOT_NULL(p, "should find process");

	// Free the process
	proc_free(p);

	unsigned long after = pmem_free_count();
	ASSERT_EQ(before, after, "all pages should be freed");
	return 0;
}

TEST_SUITE(proc) {
	RUN_TEST(proc_alloc_returns_proc);
	RUN_TEST(proc_alloc_unique_pids);
	RUN_TEST(proc_create_sets_context);
	RUN_TEST(proc_has_usermode_fields);
	RUN_TEST(proc_free_releases_all_memory);
}

#endif
