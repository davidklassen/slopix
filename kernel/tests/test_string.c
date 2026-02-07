#include "test.h"
#include "string.h"

TEST(strlen_empty) {
	ASSERT_EQ(strlen(""), 0, "empty string");
	return 0;
}

TEST(strlen_basic) {
	ASSERT_EQ(strlen("hello"), 5, "hello");
	return 0;
}

TEST(strcmp_equal) {
	ASSERT_EQ(strcmp("abc", "abc"), 0, "equal strings");
	return 0;
}

TEST(strcmp_less) {
	ASSERT(strcmp("abc", "abd") < 0, "abc < abd");
	return 0;
}

TEST(strcmp_greater) {
	ASSERT(strcmp("abd", "abc") > 0, "abd > abc");
	return 0;
}

TEST(strcmp_prefix) {
	ASSERT(strcmp("ab", "abc") < 0, "ab < abc");
	return 0;
}

TEST(strncmp_equal_n) {
	ASSERT_EQ(strncmp("abcdef", "abcxxx", 3), 0, "first 3 equal");
	return 0;
}

TEST(strncmp_diff_n) {
	ASSERT(strncmp("abcdef", "abdxxx", 3) < 0, "c < d");
	return 0;
}

TEST(strncpy_basic) {
	char buf[16];
	strncpy(buf, "hello", 16);
	ASSERT_STREQ(buf, "hello");
	return 0;
}

TEST(strncpy_truncate) {
	char buf[4];
	strncpy(buf, "hello", 3);
	buf[3] = '\0';
	ASSERT_STREQ(buf, "hel");
	return 0;
}

TEST(strncpy_pads_null) {
	char buf[8] = "xxxxxxx";
	strncpy(buf, "hi", 6);
	ASSERT_EQ(buf[2], '\0', "null at 2");
	ASSERT_EQ(buf[5], '\0', "null at 5");
	return 0;
}

TEST(strchr_found) {
	const char *s = "hello";
	ASSERT_EQ(strchr(s, 'l'), s + 2, "first l");
	return 0;
}

TEST(strchr_not_found) {
	ASSERT_NULL(strchr("hello", 'x'), "x not found");
	return 0;
}

TEST(strchr_null_terminator) {
	const char *s = "hello";
	ASSERT_EQ(strchr(s, '\0'), s + 5, "null terminator");
	return 0;
}

TEST(memset_basic) {
	char buf[8];
	memset(buf, 'A', 4);
	buf[4] = '\0';
	ASSERT_STREQ(buf, "AAAA");
	return 0;
}

TEST(memset_zero) {
	char buf[4] = "xxx";
	memset(buf, 0, 4);
	ASSERT_EQ(buf[0], 0, "zeroed");
	ASSERT_EQ(buf[3], 0, "zeroed");
	return 0;
}

TEST(memcpy_basic) {
	char src[] = "hello";
	char dst[8];
	memcpy(dst, src, 6);
	ASSERT_STREQ(dst, "hello");
	return 0;
}

TEST(memcpy_partial) {
	char src[] = "hello";
	char dst[8] = "xxxxx";
	memcpy(dst, src, 3);
	dst[3] = '\0';
	ASSERT_STREQ(dst, "hel");
	return 0;
}

TEST(memmove_non_overlap) {
	char src[] = "hello";
	char dst[8];
	memmove(dst, src, 6);
	ASSERT_STREQ(dst, "hello");
	return 0;
}

TEST(memmove_overlap_forward) {
	char buf[] = "hello";
	memmove(buf + 2, buf, 3);
	ASSERT_EQ(buf[2], 'h', "h moved");
	ASSERT_EQ(buf[3], 'e', "e moved");
	ASSERT_EQ(buf[4], 'l', "l moved");
	return 0;
}

TEST(memmove_overlap_backward) {
	char buf[] = "hello";
	memmove(buf, buf + 2, 3);
	ASSERT_EQ(buf[0], 'l', "l moved");
	ASSERT_EQ(buf[1], 'l', "l moved");
	ASSERT_EQ(buf[2], 'o', "o moved");
	return 0;
}

TEST_SUITE(string) {
	RUN_TEST(strlen_empty);
	RUN_TEST(strlen_basic);
	RUN_TEST(strcmp_equal);
	RUN_TEST(strcmp_less);
	RUN_TEST(strcmp_greater);
	RUN_TEST(strcmp_prefix);
	RUN_TEST(strncmp_equal_n);
	RUN_TEST(strncmp_diff_n);
	RUN_TEST(strncpy_basic);
	RUN_TEST(strncpy_truncate);
	RUN_TEST(strncpy_pads_null);
	RUN_TEST(strchr_found);
	RUN_TEST(strchr_not_found);
	RUN_TEST(strchr_null_terminator);
	RUN_TEST(memset_basic);
	RUN_TEST(memset_zero);
	RUN_TEST(memcpy_basic);
	RUN_TEST(memcpy_partial);
	RUN_TEST(memmove_non_overlap);
	RUN_TEST(memmove_overlap_forward);
	RUN_TEST(memmove_overlap_backward);
}
