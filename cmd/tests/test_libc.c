#include <ctype.h>
#include <string.h>
#include <test.h>

TEST(strlen_empty) {
	ASSERT_EQ(strlen(""), 0, "empty string");
	return 0;
}

TEST(strlen_basic) {
	ASSERT_EQ(strlen("hello"), 5, "hello length");
	return 0;
}

TEST(strcmp_equal) {
	ASSERT_EQ(strcmp("hello", "hello"), 0, "equal strings");
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
	ASSERT(strcmp("hello", "hell") > 0, "hello > hell");
	return 0;
}

TEST(strncmp_equal_n) {
	ASSERT_EQ(strncmp("hello", "help", 3), 0, "equal prefix");
	return 0;
}

TEST(strncmp_diff_n) {
	ASSERT(strncmp("hello", "help", 4) != 0, "different at 4");
	return 0;
}

TEST(strcpy_basic) {
	char buf[16];
	strcpy(buf, "test");
	ASSERT_EQ(strcmp(buf, "test"), 0, "copied correctly");
	return 0;
}

TEST(strncpy_basic) {
	char buf[16];
	memset(buf, 'x', 16);
	strncpy(buf, "hello", 16);
	ASSERT_EQ(strcmp(buf, "hello"), 0, "copied correctly");
	return 0;
}

TEST(strncpy_truncate) {
	char buf[4];
	strncpy(buf, "hello", 3);
	buf[3] = '\0';
	ASSERT_EQ(strcmp(buf, "hel"), 0, "truncated to 3");
	return 0;
}

TEST(strncpy_pads_null) {
	char buf[8];
	memset(buf, 'x', 8);
	strncpy(buf, "hi", 8);
	ASSERT_EQ(buf[2], '\0', "null at 2");
	ASSERT_EQ(buf[7], '\0', "null padded to end");
	return 0;
}

TEST(strcat_basic) {
	char buf[16] = "hello";
	strcat(buf, " world");
	ASSERT_EQ(strcmp(buf, "hello world"), 0, "concatenated");
	return 0;
}

TEST(strchr_found) {
	char *p = strchr("hello", 'e');
	ASSERT(p != 0, "found e");
	ASSERT_EQ(*p, 'e', "points to e");
	return 0;
}

TEST(strchr_not_found) {
	ASSERT_EQ((long)strchr("hello", 'x'), 0, "x not found");
	return 0;
}

TEST(strchr_null_terminator) {
	char *p = strchr("hello", '\0');
	ASSERT(p != 0, "found null");
	ASSERT_EQ(*p, '\0', "points to null");
	return 0;
}

TEST(strstr_found) {
	char *p = strstr("hello world", "world");
	ASSERT(p != 0, "found world");
	ASSERT_EQ(strcmp(p, "world"), 0, "points to world");
	return 0;
}

TEST(strstr_not_found) {
	ASSERT_EQ((long)strstr("hello", "xyz"), 0, "xyz not found");
	return 0;
}

TEST(strstr_empty_needle) {
	char *p = strstr("hello", "");
	ASSERT(p != 0, "empty needle found");
	ASSERT_EQ(strcmp(p, "hello"), 0, "returns haystack");
	return 0;
}

TEST(memset_basic) {
	char buf[8];
	memset(buf, 'x', 8);
	for (int i = 0; i < 8; i++) {
		ASSERT_EQ(buf[i], 'x', "filled with x");
	}
	return 0;
}

TEST(memset_zero) {
	char buf[8] = "hello!!";
	memset(buf, 0, 8);
	for (int i = 0; i < 8; i++) {
		ASSERT_EQ(buf[i], 0, "zeroed");
	}
	return 0;
}

TEST(memcpy_basic) {
	char src[] = "hello";
	char dst[8];
	memcpy(dst, src, 6);
	ASSERT_EQ(strcmp(dst, "hello"), 0, "copied");
	return 0;
}

TEST(memmove_non_overlap) {
	char src[] = "hello";
	char dst[8];
	memmove(dst, src, 6);
	ASSERT_EQ(strcmp(dst, "hello"), 0, "copied");
	return 0;
}

TEST(memmove_overlap_forward) {
	char buf[16] = "hello";
	memmove(buf + 2, buf, 5);
	ASSERT_EQ(buf[2], 'h', "h at 2");
	ASSERT_EQ(buf[3], 'e', "e at 3");
	return 0;
}

TEST(memmove_overlap_backward) {
	char buf[16] = "  hello";
	memmove(buf, buf + 2, 5);
	ASSERT_EQ(buf[0], 'h', "h at 0");
	ASSERT_EQ(buf[1], 'e', "e at 1");
	return 0;
}

TEST(atoi_positive) {
	ASSERT_EQ(atoi("123"), 123, "123");
	return 0;
}

TEST(atoi_negative) {
	ASSERT_EQ(atoi("-42"), -42, "-42");
	return 0;
}

TEST(atoi_whitespace) {
	ASSERT_EQ(atoi("  99"), 99, "leading spaces");
	return 0;
}

TEST(atoi_zero) {
	ASSERT_EQ(atoi("0"), 0, "zero");
	return 0;
}

TEST(itoa_positive) {
	char buf[16];
	itoa(123, buf);
	ASSERT_EQ(strcmp(buf, "123"), 0, "123");
	return 0;
}

TEST(itoa_negative) {
	char buf[16];
	itoa(-42, buf);
	ASSERT_EQ(strcmp(buf, "-42"), 0, "-42");
	return 0;
}

TEST(itoa_zero) {
	char buf[16];
	itoa(0, buf);
	ASSERT_EQ(strcmp(buf, "0"), 0, "0");
	return 0;
}

TEST(isspace_space) {
	ASSERT(isspace(' '), "space");
	return 0;
}

TEST(isspace_tab) {
	ASSERT(isspace('\t'), "tab");
	return 0;
}

TEST(isspace_newline) {
	ASSERT(isspace('\n'), "newline");
	return 0;
}

TEST(isspace_not) {
	ASSERT(!isspace('a'), "a not whitespace");
	ASSERT(!isspace('0'), "0 not whitespace");
	return 0;
}

TEST(isdigit_digits) {
	ASSERT(isdigit('0'), "0 is digit");
	ASSERT(isdigit('5'), "5 is digit");
	ASSERT(isdigit('9'), "9 is digit");
	return 0;
}

TEST(isdigit_not) {
	ASSERT(!isdigit('a'), "a not digit");
	ASSERT(!isdigit(' '), "space not digit");
	return 0;
}

TEST(isalpha_lower) {
	ASSERT(isalpha('a'), "a is alpha");
	ASSERT(isalpha('z'), "z is alpha");
	return 0;
}

TEST(isalpha_upper) {
	ASSERT(isalpha('A'), "A is alpha");
	ASSERT(isalpha('Z'), "Z is alpha");
	return 0;
}

TEST(isalpha_not) {
	ASSERT(!isalpha('0'), "0 not alpha");
	ASSERT(!isalpha(' '), "space not alpha");
	return 0;
}

TEST_SUITE(libc) {
	RUN_TEST(strlen_empty);
	RUN_TEST(strlen_basic);
	RUN_TEST(strcmp_equal);
	RUN_TEST(strcmp_less);
	RUN_TEST(strcmp_greater);
	RUN_TEST(strcmp_prefix);
	RUN_TEST(strncmp_equal_n);
	RUN_TEST(strncmp_diff_n);
	RUN_TEST(strcpy_basic);
	RUN_TEST(strncpy_basic);
	RUN_TEST(strncpy_truncate);
	RUN_TEST(strncpy_pads_null);
	RUN_TEST(strcat_basic);
	RUN_TEST(strchr_found);
	RUN_TEST(strchr_not_found);
	RUN_TEST(strchr_null_terminator);
	RUN_TEST(strstr_found);
	RUN_TEST(strstr_not_found);
	RUN_TEST(strstr_empty_needle);
	RUN_TEST(memset_basic);
	RUN_TEST(memset_zero);
	RUN_TEST(memcpy_basic);
	RUN_TEST(memmove_non_overlap);
	RUN_TEST(memmove_overlap_forward);
	RUN_TEST(memmove_overlap_backward);
	RUN_TEST(atoi_positive);
	RUN_TEST(atoi_negative);
	RUN_TEST(atoi_whitespace);
	RUN_TEST(atoi_zero);
	RUN_TEST(itoa_positive);
	RUN_TEST(itoa_negative);
	RUN_TEST(itoa_zero);
	RUN_TEST(isspace_space);
	RUN_TEST(isspace_tab);
	RUN_TEST(isspace_newline);
	RUN_TEST(isspace_not);
	RUN_TEST(isdigit_digits);
	RUN_TEST(isdigit_not);
	RUN_TEST(isalpha_lower);
	RUN_TEST(isalpha_upper);
	RUN_TEST(isalpha_not);
}
