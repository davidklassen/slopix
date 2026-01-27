#include <stdlib.h>
#include <string.h>
#include <test.h>

TEST(malloc_basic) {
	void *p = malloc(64);
	ASSERT_NOT_NULL(p, "alloc succeeded");
	ASSERT_EQ((unsigned long)p % 16, 0, "16-byte aligned");
	free(p);
	return 0;
}

TEST(malloc_zero) {
	void *p = malloc(0);
	ASSERT_NULL(p, "malloc(0) returns NULL");
	return 0;
}

TEST(free_null) {
	free(NULL);
	return 0;
}

TEST(calloc_zeroed) {
	unsigned char *p = calloc(16, 1);
	ASSERT_NOT_NULL(p, "calloc succeeded");
	for (int i = 0; i < 16; i++) {
		ASSERT_EQ(p[i], 0, "memory zeroed");
	}
	free(p);
	return 0;
}

TEST(realloc_null) {
	void *p = realloc(NULL, 32);
	ASSERT_NOT_NULL(p, "realloc(NULL, n) works");
	free(p);
	return 0;
}

TEST(realloc_zero) {
	void *p = malloc(32);
	ASSERT_NOT_NULL(p, "malloc succeeded");
	void *q = realloc(p, 0);
	ASSERT_NULL(q, "realloc(p, 0) returns NULL");
	return 0;
}

TEST(realloc_grow) {
	char *p = malloc(16);
	ASSERT_NOT_NULL(p, "malloc succeeded");
	memcpy(p, "hello", 6);
	char *q = realloc(p, 64);
	ASSERT_NOT_NULL(q, "realloc succeeded");
	ASSERT_EQ(strcmp(q, "hello"), 0, "data preserved");
	free(q);
	return 0;
}

TEST(malloc_multiple) {
	void *ptrs[8];
	for (int i = 0; i < 8; i++) {
		ptrs[i] = malloc(32);
		ASSERT_NOT_NULL(ptrs[i], "alloc succeeded");
	}
	for (int i = 0; i < 8; i++) {
		free(ptrs[i]);
	}
	return 0;
}

TEST(malloc_reuse) {
	void *p1 = malloc(64);
	ASSERT_NOT_NULL(p1, "first alloc");
	free(p1);
	void *p2 = malloc(64);
	ASSERT_NOT_NULL(p2, "second alloc");
	ASSERT_EQ((unsigned long)p1, (unsigned long)p2, "memory reused");
	free(p2);
	return 0;
}

TEST(malloc_large) {
	void *p = malloc(65536);
	ASSERT_NOT_NULL(p, "64KB alloc");
	free(p);
	return 0;
}

TEST_SUITE(malloc) {
	RUN_TEST(malloc_basic);
	RUN_TEST(malloc_zero);
	RUN_TEST(free_null);
	RUN_TEST(calloc_zeroed);
	RUN_TEST(realloc_null);
	RUN_TEST(realloc_zero);
	RUN_TEST(realloc_grow);
	RUN_TEST(malloc_multiple);
	RUN_TEST(malloc_reuse);
	RUN_TEST(malloc_large);
}
