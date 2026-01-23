#include <test.h>
#include <unistd.h>

TEST(sbrk_returns_old_break) {
	void *p1 = sbrk(0);
	void *p2 = sbrk(0);
	ASSERT_EQ((long)p1, (long)p2, "sbrk(0) returns same address");
	return 0;
}

TEST(sbrk_grows_heap) {
	void *old = sbrk(4096);
	ASSERT(old != (void *)-1, "sbrk(4096) succeeds");
	void *new = sbrk(0);
	ASSERT_EQ((long)new - (long)old, 4096, "heap grew by 4096");
	return 0;
}

TEST(sbrk_memory_usable) {
	char *p = sbrk(100);
	ASSERT(p != (char *)-1, "sbrk succeeds");
	for (int i = 0; i < 100; i++) {
		p[i] = (char)i;
	}
	for (int i = 0; i < 100; i++) {
		ASSERT_EQ(p[i], (char)i, "memory holds value");
	}
	return 0;
}

TEST_SUITE(memory) {
	RUN_TEST(sbrk_returns_old_break);
	RUN_TEST(sbrk_grows_heap);
	RUN_TEST(sbrk_memory_usable);
}
