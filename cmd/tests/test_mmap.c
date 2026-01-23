#include <test.h>
#include <sys/mman.h>

TEST(mmap_basic) {
	void *p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT(p != MAP_FAILED, "mmap succeeds");
	ASSERT(p != 0, "mmap returns non-null");

	char *cp = (char *)p;
	cp[0] = 0x42;
	ASSERT_EQ(cp[0], 0x42, "can write and read mmap'd memory");

	int r = munmap(p, 4096);
	ASSERT_EQ(r, 0, "munmap succeeds");
	return 0;
}

TEST(mmap_zeroed) {
	void *p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	ASSERT(p != MAP_FAILED, "mmap succeeds");

	char *cp = (char *)p;
	int all_zero = 1;
	for (int i = 0; i < 4096; i++) {
		if (cp[i] != 0) {
			all_zero = 0;
			break;
		}
	}
	ASSERT(all_zero, "mmap'd memory is zeroed");

	munmap(p, 4096);
	return 0;
}

TEST(mmap_fixed) {
	void *addr = (void *)0x100000;
	void *p = mmap(addr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	ASSERT(p != MAP_FAILED, "mmap MAP_FIXED succeeds");
	ASSERT_EQ(p, addr, "mmap MAP_FIXED returns requested address");

	char *cp = (char *)p;
	cp[0] = 0x99;
	ASSERT_EQ(cp[0], 0x99, "can access fixed mmap'd memory");

	munmap(p, 4096);
	return 0;
}

TEST(mmap_tlb_invalidation) {
	void *addr = (void *)0x200000;

	void *p1 = mmap(addr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	ASSERT(p1 != MAP_FAILED, "first mmap succeeds");
	ASSERT_EQ(p1, addr, "first mmap returns requested address");

	volatile char *cp = (volatile char *)p1;
	cp[0] = 0xAB;
	ASSERT_EQ(cp[0], (char)0xAB, "wrote marker value");

	int r = munmap(p1, 4096);
	ASSERT_EQ(r, 0, "munmap succeeds");

	void *p2 = mmap(addr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	ASSERT(p2 != MAP_FAILED, "second mmap succeeds");
	ASSERT_EQ(p2, addr, "second mmap returns same address");

	volatile char *cp2 = (volatile char *)p2;
	char val = cp2[0];

	ASSERT_EQ(val, 0, "newly mapped page should be zeroed (TLB was invalidated)");

	munmap(p2, 4096);
	return 0;
}

TEST_SUITE(mmap) {
	RUN_TEST(mmap_basic);
	RUN_TEST(mmap_zeroed);
	RUN_TEST(mmap_fixed);
	RUN_TEST(mmap_tlb_invalidation);
}
