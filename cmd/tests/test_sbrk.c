#include <test.h>
#include <errno.h>
#include <unistd.h>

TEST(sbrk_returns_old_break) {
	void *p1 = sbrk(0);
	ASSERT(p1 != (void *)-1, "sbrk(0) succeeds");
	void *p2 = sbrk(0);
	ASSERT_EQ(p1, p2, "sbrk(0) returns same value");
	return 0;
}

TEST(sbrk_increases_break) {
	void *old = sbrk(0);
	void *ret = sbrk(4096);
	ASSERT_EQ(ret, old, "sbrk returns old break");
	void *new = sbrk(0);
	ASSERT_EQ((char *)new - (char *)old, 4096, "break increased by 4096");
	return 0;
}

TEST(sbrk_memory_is_usable) {
	void *p = sbrk(4096);
	ASSERT(p != (void *)-1, "sbrk succeeds");
	char *cp = (char *)p;
	for (int i = 0; i < 4096; i++) {
		cp[i] = (char)(i & 0xff);
	}
	for (int i = 0; i < 4096; i++) {
		ASSERT_EQ(cp[i], (char)(i & 0xff), "memory contents match");
	}
	return 0;
}

TEST(sbrk_multiple_calls) {
	void *p1 = sbrk(1024);
	ASSERT(p1 != (void *)-1, "first sbrk succeeds");
	void *p2 = sbrk(1024);
	ASSERT(p2 != (void *)-1, "second sbrk succeeds");
	void *p3 = sbrk(1024);
	ASSERT(p3 != (void *)-1, "third sbrk succeeds");
	ASSERT_EQ((char *)p2 - (char *)p1, 1024, "p2 follows p1");
	ASSERT_EQ((char *)p3 - (char *)p2, 1024, "p3 follows p2");
	return 0;
}

TEST(sbrk_zero_is_noop) {
	void *before = sbrk(0);
	void *ret = sbrk(0);
	void *after = sbrk(0);
	ASSERT_EQ(ret, before, "sbrk(0) returns current break");
	ASSERT_EQ(before, after, "break unchanged");
	return 0;
}

TEST(sbrk_negative_shrinks) {
	void *initial = sbrk(0);
	sbrk(4096);
	void *after_grow = sbrk(0);
	ASSERT_EQ((char *)after_grow - (char *)initial, 4096, "grew by 4096");
	void *ret = sbrk(-2048);
	ASSERT_EQ(ret, after_grow, "sbrk returns old break");
	void *after_shrink = sbrk(0);
	ASSERT_EQ((char *)after_shrink - (char *)initial, 2048, "shrunk to 2048");
	return 0;
}

TEST(sbrk_underflow_fails) {
	errno = 0;
	void *ret = sbrk(-0x7FFFFFFFFFFFFFFF);
	ASSERT_EQ(ret, (void *)-1, "massive negative fails");
	ASSERT_EQ(errno, EINVAL, "errno is EINVAL");
	return 0;
}

TEST_SUITE(sbrk) {
	RUN_TEST(sbrk_returns_old_break);
	RUN_TEST(sbrk_increases_break);
	RUN_TEST(sbrk_memory_is_usable);
	RUN_TEST(sbrk_multiple_calls);
	RUN_TEST(sbrk_zero_is_noop);
	RUN_TEST(sbrk_negative_shrinks);
	RUN_TEST(sbrk_underflow_fails);
}
