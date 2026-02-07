#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <test.h>
#include <time.h>
#include <unistd.h>

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

TEST(ispunct_symbols) {
	ASSERT(ispunct('!'), "! is punct");
	ASSERT(ispunct('.'), ". is punct");
	ASSERT(ispunct('@'), "@ is punct");
	ASSERT(ispunct('['), "[ is punct");
	ASSERT(ispunct('~'), "~ is punct");
	return 0;
}

TEST(ispunct_not) {
	ASSERT(!ispunct('a'), "a not punct");
	ASSERT(!ispunct('0'), "0 not punct");
	ASSERT(!ispunct(' '), "space not punct");
	return 0;
}

TEST(isalnum_alpha) {
	ASSERT(isalnum('a'), "a is alnum");
	ASSERT(isalnum('Z'), "Z is alnum");
	return 0;
}

TEST(isalnum_digit) {
	ASSERT(isalnum('0'), "0 is alnum");
	ASSERT(isalnum('9'), "9 is alnum");
	return 0;
}

TEST(isalnum_not) {
	ASSERT(!isalnum(' '), "space not alnum");
	ASSERT(!isalnum('!'), "! not alnum");
	return 0;
}

TEST(isxdigit_digits) {
	ASSERT(isxdigit('0'), "0 is xdigit");
	ASSERT(isxdigit('9'), "9 is xdigit");
	return 0;
}

TEST(isxdigit_hex) {
	ASSERT(isxdigit('a'), "a is xdigit");
	ASSERT(isxdigit('f'), "f is xdigit");
	ASSERT(isxdigit('A'), "A is xdigit");
	ASSERT(isxdigit('F'), "F is xdigit");
	return 0;
}

TEST(isxdigit_not) {
	ASSERT(!isxdigit('g'), "g not xdigit");
	ASSERT(!isxdigit('G'), "G not xdigit");
	return 0;
}

TEST(isupper_test) {
	ASSERT(isupper('A'), "A is upper");
	ASSERT(isupper('Z'), "Z is upper");
	ASSERT(!isupper('a'), "a not upper");
	ASSERT(!isupper('0'), "0 not upper");
	return 0;
}

TEST(islower_test) {
	ASSERT(islower('a'), "a is lower");
	ASSERT(islower('z'), "z is lower");
	ASSERT(!islower('A'), "A not lower");
	ASSERT(!islower('0'), "0 not lower");
	return 0;
}

TEST(tolower_test) {
	ASSERT_EQ(tolower('A'), 'a', "A -> a");
	ASSERT_EQ(tolower('Z'), 'z', "Z -> z");
	ASSERT_EQ(tolower('a'), 'a', "a unchanged");
	ASSERT_EQ(tolower('0'), '0', "0 unchanged");
	return 0;
}

TEST(toupper_test) {
	ASSERT_EQ(toupper('a'), 'A', "a -> A");
	ASSERT_EQ(toupper('z'), 'Z', "z -> Z");
	ASSERT_EQ(toupper('A'), 'A', "A unchanged");
	ASSERT_EQ(toupper('0'), '0', "0 unchanged");
	return 0;
}

TEST(strdup_basic) {
	char *s = strdup("hello");
	ASSERT_NOT_NULL(s, "strdup returned NULL");
	ASSERT_EQ(strcmp(s, "hello"), 0, "content matches");
	free(s);
	return 0;
}

TEST(strdup_empty) {
	char *s = strdup("");
	ASSERT_NOT_NULL(s, "strdup empty");
	ASSERT_EQ(strlen(s), 0, "empty string");
	free(s);
	return 0;
}

TEST(strndup_basic) {
	char *s = strndup("hello world", 5);
	ASSERT_NOT_NULL(s, "strndup returned NULL");
	ASSERT_EQ(strcmp(s, "hello"), 0, "truncated at 5");
	free(s);
	return 0;
}

TEST(strndup_longer) {
	char *s = strndup("hi", 10);
	ASSERT_NOT_NULL(s, "strndup returned NULL");
	ASSERT_EQ(strcmp(s, "hi"), 0, "full string when n > len");
	free(s);
	return 0;
}

TEST(strrchr_found) {
	const char *s = "/path/to/file.txt";
	char *p = strrchr(s, '/');
	ASSERT_NOT_NULL(p, "found /");
	ASSERT_EQ(strcmp(p, "/file.txt"), 0, "last slash");
	return 0;
}

TEST(strrchr_not_found) {
	ASSERT_NULL(strrchr("hello", 'x'), "x not found");
	return 0;
}

TEST(strrchr_null_terminator) {
	const char *s = "hello";
	char *p = strrchr(s, '\0');
	ASSERT_NOT_NULL(p, "found null");
	ASSERT_EQ(*p, '\0', "points to null");
	return 0;
}

TEST(strtok_basic) {
	char buf[] = "a,b,c";
	char *tok = strtok(buf, ",");
	ASSERT_NOT_NULL(tok, "first token");
	ASSERT_EQ(strcmp(tok, "a"), 0, "first is a");
	tok = strtok(0, ",");
	ASSERT_NOT_NULL(tok, "second token");
	ASSERT_EQ(strcmp(tok, "b"), 0, "second is b");
	tok = strtok(0, ",");
	ASSERT_NOT_NULL(tok, "third token");
	ASSERT_EQ(strcmp(tok, "c"), 0, "third is c");
	tok = strtok(0, ",");
	ASSERT_NULL(tok, "no more tokens");
	return 0;
}

TEST(strtok_multiple_delims) {
	char buf[] = "a,,b";
	char *tok = strtok(buf, ",");
	ASSERT_EQ(strcmp(tok, "a"), 0, "first is a");
	tok = strtok(0, ",");
	ASSERT_EQ(strcmp(tok, "b"), 0, "skips empty");
	return 0;
}

TEST(strtok_leading_delim) {
	char buf[] = ",a,b";
	char *tok = strtok(buf, ",");
	ASSERT_EQ(strcmp(tok, "a"), 0, "skips leading");
	return 0;
}

TEST(strtok_r_basic) {
	char buf[] = "a,b,c";
	char *saveptr;
	char *tok = strtok_r(buf, ",", &saveptr);
	ASSERT_EQ(strcmp(tok, "a"), 0, "first token");
	tok = strtok_r(0, ",", &saveptr);
	ASSERT_EQ(strcmp(tok, "b"), 0, "second token");
	tok = strtok_r(0, ",", &saveptr);
	ASSERT_EQ(strcmp(tok, "c"), 0, "third token");
	tok = strtok_r(0, ",", &saveptr);
	ASSERT_EQ(tok, 0, "null after end");
	return 0;
}

TEST(strtok_r_nested) {
	char buf1[] = "a:b c:d";
	char buf2[16];
	char *saveptr1, *saveptr2;
	char *outer = strtok_r(buf1, " ", &saveptr1);
	strcpy(buf2, outer);
	char *inner = strtok_r(buf2, ":", &saveptr2);
	ASSERT_EQ(strcmp(inner, "a"), 0, "first inner");
	inner = strtok_r(0, ":", &saveptr2);
	ASSERT_EQ(strcmp(inner, "b"), 0, "second inner");
	outer = strtok_r(0, " ", &saveptr1);
	strcpy(buf2, outer);
	inner = strtok_r(buf2, ":", &saveptr2);
	ASSERT_EQ(strcmp(inner, "c"), 0, "third inner");
	return 0;
}

TEST(strcasecmp_equal) {
	ASSERT_EQ(strcasecmp("hello", "hello"), 0, "equal lowercase");
	ASSERT_EQ(strcasecmp("HELLO", "hello"), 0, "upper vs lower");
	ASSERT_EQ(strcasecmp("HeLLo", "hElLO"), 0, "mixed case");
	return 0;
}

TEST(strcasecmp_less) {
	ASSERT(strcasecmp("abc", "ABD") < 0, "abc < ABD");
	ASSERT(strcasecmp("ABC", "abd") < 0, "ABC < abd");
	return 0;
}

TEST(strcasecmp_greater) {
	ASSERT(strcasecmp("abd", "ABC") > 0, "abd > ABC");
	ASSERT(strcasecmp("ABD", "abc") > 0, "ABD > abc");
	return 0;
}

TEST(strcasecmp_prefix) {
	ASSERT(strcasecmp("HELLO", "hell") > 0, "HELLO > hell");
	ASSERT(strcasecmp("hell", "HELLO") < 0, "hell < HELLO");
	return 0;
}

TEST(strncasecmp_equal_n) {
	ASSERT_EQ(strncasecmp("HELLO", "help", 3), 0, "equal prefix");
	ASSERT_EQ(strncasecmp("hello", "HELP", 3), 0, "equal prefix mixed");
	return 0;
}

TEST(strncasecmp_diff_n) {
	ASSERT(strncasecmp("HELLO", "help", 4) != 0, "different at 4");
	return 0;
}

TEST(strncasecmp_zero_n) {
	ASSERT_EQ(strncasecmp("abc", "xyz", 0), 0, "n=0 always equal");
	return 0;
}

TEST(strtoul_decimal) {
	ASSERT_EQ(strtoul("123", NULL, 10), 123, "decimal");
	ASSERT_EQ(strtoul("  456", NULL, 10), 456, "leading space");
	ASSERT_EQ(strtoul("+789", NULL, 10), 789, "plus sign");
	return 0;
}

TEST(strtoul_hex) {
	ASSERT_EQ(strtoul("0xff", NULL, 0), 255, "auto hex lowercase");
	ASSERT_EQ(strtoul("0XFF", NULL, 0), 255, "auto hex uppercase");
	ASSERT_EQ(strtoul("ff", NULL, 16), 255, "explicit hex no prefix");
	ASSERT_EQ(strtoul("0xff", NULL, 16), 255, "explicit hex with prefix");
	return 0;
}

TEST(strtoul_octal) {
	ASSERT_EQ(strtoul("0777", NULL, 0), 511, "auto octal");
	ASSERT_EQ(strtoul("777", NULL, 8), 511, "explicit octal");
	return 0;
}

TEST(strtoul_endptr) {
	char *end;
	ASSERT_EQ(strtoul("123abc", &end, 10), 123, "value");
	ASSERT_EQ(*end, 'a', "endptr points to first non-digit");
	return 0;
}

TEST(strtol_negative) {
	ASSERT_EQ(strtol("-42", NULL, 10), -42, "negative decimal");
	ASSERT_EQ(strtol("  -100", NULL, 10), -100, "negative with space");
	return 0;
}

TEST(strtol_zero) {
	ASSERT_EQ(strtol("0", NULL, 10), 0, "zero");
	ASSERT_EQ(strtol("0x0", NULL, 0), 0, "hex zero");
	return 0;
}

TEST(errno_initial) {
	errno = 0;
	ASSERT_EQ(errno, 0, "errno can be set to 0");
	return 0;
}

TEST(errno_set) {
	errno = ENOENT;
	ASSERT_EQ(errno, 2, "errno set to ENOENT (2)");
	errno = EINVAL;
	ASSERT_EQ(errno, 22, "errno set to EINVAL (22)");
	errno = 0;
	return 0;
}

TEST(strerror_success) {
	char *msg = strerror(0);
	ASSERT_NOT_NULL(msg, "strerror(0) not null");
	ASSERT_EQ(strcmp(msg, "Success"), 0, "strerror(0) is Success");
	return 0;
}

TEST(strerror_enoent) {
	char *msg = strerror(ENOENT);
	ASSERT_NOT_NULL(msg, "strerror(ENOENT) not null");
	ASSERT_EQ(strcmp(msg, "No such file or directory"), 0, "ENOENT message");
	return 0;
}

TEST(strerror_einval) {
	char *msg = strerror(EINVAL);
	ASSERT_NOT_NULL(msg, "strerror(EINVAL) not null");
	ASSERT_EQ(strcmp(msg, "Invalid argument"), 0, "EINVAL message");
	return 0;
}

TEST(strerror_unknown) {
	char *msg = strerror(999);
	ASSERT_NOT_NULL(msg, "strerror(999) not null");
	ASSERT_EQ(strcmp(msg, "Unknown error"), 0, "unknown returns Unknown error");
	return 0;
}

TEST(strerror_negative) {
	char *msg = strerror(-1);
	ASSERT_NOT_NULL(msg, "strerror(-1) not null");
	ASSERT_EQ(strcmp(msg, "Unknown error"), 0, "negative returns Unknown error");
	return 0;
}

TEST(mkstemp_basic) {
	char templ[] = "/tmpXXXXXX";
	int fd = mkstemp(templ);
	ASSERT(fd >= 0, "mkstemp returns valid fd");
	ASSERT(templ[4] != 'X', "template modified");
	close(fd);
	unlink(templ);
	return 0;
}

TEST(mkstemp_write_read) {
	char templ[] = "/tmpXXXXXX";
	int fd = mkstemp(templ);
	ASSERT(fd >= 0, "mkstemp returns valid fd");

	write(fd, "test", 4);
	lseek(fd, 0, SEEK_SET);
	char buf[10] = {0};
	read(fd, buf, 4);
	ASSERT_EQ(strcmp(buf, "test"), 0, "file content");

	close(fd);
	unlink(templ);
	return 0;
}

TEST(mkstemp_unique) {
	char t1[] = "/tmpXXXXXX";
	char t2[] = "/tmpXXXXXX";
	int fd1 = mkstemp(t1);
	int fd2 = mkstemp(t2);
	ASSERT(fd1 >= 0, "first mkstemp valid");
	ASSERT(fd2 >= 0, "second mkstemp valid");
	ASSERT(strcmp(t1, t2) != 0, "templates are different");
	close(fd1);
	close(fd2);
	unlink(t1);
	unlink(t2);
	return 0;
}

TEST(mkstemp_invalid_template) {
	char short_templ[] = "/tmp";
	ASSERT_EQ(mkstemp(short_templ), -1, "short template fails");

	char no_x_templ[] = "/tmpABCDEF";
	ASSERT_EQ(mkstemp(no_x_templ), -1, "no X template fails");
	return 0;
}

static void dummy_handler(void) {
}

TEST(atexit_register) {
	ASSERT_EQ(atexit(dummy_handler), 0, "atexit returns 0");
	return 0;
}

TEST(time_returns_zero) {
	ASSERT_EQ(time(0), 0, "time returns 0");
	return 0;
}

TEST(time_sets_tloc) {
	time_t t = 99;
	time(&t);
	ASSERT_EQ(t, 0, "time sets tloc to 0");
	return 0;
}

TEST(localtime_returns_valid) {
	time_t t = 0;
	struct tm *tm = localtime(&t);
	ASSERT_NOT_NULL(tm, "localtime returns non-null");
	return 0;
}

TEST(localtime_zeroed) {
	time_t t = 0;
	struct tm *tm = localtime(&t);
	ASSERT_EQ(tm->tm_sec, 0, "tm_sec is 0");
	ASSERT_EQ(tm->tm_min, 0, "tm_min is 0");
	ASSERT_EQ(tm->tm_hour, 0, "tm_hour is 0");
	return 0;
}

TEST(isprint_printable) {
	ASSERT(isprint(' '), "space is printable");
	ASSERT(isprint('a'), "a is printable");
	ASSERT(isprint('~'), "~ is printable");
	return 0;
}

TEST(isprint_not_printable) {
	ASSERT(!isprint('\0'), "null not printable");
	ASSERT(!isprint('\n'), "newline not printable");
	ASSERT(!isprint(0x1f), "control char not printable");
	ASSERT(!isprint(0x7f), "DEL not printable");
	return 0;
}

TEST(isatty_console) {
	ASSERT(isatty(0), "stdin is tty");
	ASSERT(isatty(1), "stdout is tty");
	ASSERT(isatty(2), "stderr is tty");
	return 0;
}

TEST(isatty_file) {
	int fd = open("/tmp_isatty", O_CREAT | O_WRONLY);
	ASSERT(!isatty(fd), "file is not tty");
	close(fd);
	unlink("/tmp_isatty");
	return 0;
}

TEST(getenv_unset) {
	ASSERT_NULL(getenv("NONEXISTENT_VAR"), "unset var returns NULL");
	ASSERT_NULL(getenv(""), "empty name returns NULL");
	ASSERT_NULL(getenv(NULL), "NULL name returns NULL");
	return 0;
}

TEST(setenv_basic) {
	ASSERT_EQ(setenv("TEST_VAR", "hello", 1), 0, "setenv returns 0");
	char *val = getenv("TEST_VAR");
	ASSERT_NOT_NULL(val, "getenv finds it");
	ASSERT_EQ(strcmp(val, "hello"), 0, "value matches");
	unsetenv("TEST_VAR");
	return 0;
}

TEST(setenv_overwrite) {
	setenv("TEST_VAR", "first", 1);
	setenv("TEST_VAR", "second", 1);
	char *val = getenv("TEST_VAR");
	ASSERT_EQ(strcmp(val, "second"), 0, "overwritten");
	unsetenv("TEST_VAR");
	return 0;
}

TEST(setenv_no_overwrite) {
	setenv("TEST_VAR", "first", 1);
	setenv("TEST_VAR", "second", 0);
	char *val = getenv("TEST_VAR");
	ASSERT_EQ(strcmp(val, "first"), 0, "not overwritten");
	unsetenv("TEST_VAR");
	return 0;
}

TEST(setenv_invalid) {
	ASSERT_EQ(setenv("", "value", 1), -1, "empty name fails");
	ASSERT_EQ(setenv("A=B", "value", 1), -1, "name with = fails");
	return 0;
}

TEST(unsetenv_basic) {
	setenv("TEST_VAR", "value", 1);
	ASSERT_NOT_NULL(getenv("TEST_VAR"), "set before unset");
	ASSERT_EQ(unsetenv("TEST_VAR"), 0, "unsetenv returns 0");
	ASSERT_NULL(getenv("TEST_VAR"), "unset after unsetenv");
	return 0;
}

TEST(unsetenv_nonexistent) {
	ASSERT_EQ(unsetenv("NONEXISTENT_VAR"), 0, "unset nonexistent ok");
	return 0;
}

TEST(unsetenv_invalid) {
	ASSERT_EQ(unsetenv(""), -1, "empty name fails");
	ASSERT_EQ(unsetenv("A=B"), -1, "name with = fails");
	return 0;
}

TEST(setenv_multiple) {
	setenv("VAR1", "one", 1);
	setenv("VAR2", "two", 1);
	setenv("VAR3", "three", 1);
	ASSERT_EQ(strcmp(getenv("VAR1"), "one"), 0, "var1");
	ASSERT_EQ(strcmp(getenv("VAR2"), "two"), 0, "var2");
	ASSERT_EQ(strcmp(getenv("VAR3"), "three"), 0, "var3");
	unsetenv("VAR1");
	unsetenv("VAR2");
	unsetenv("VAR3");
	return 0;
}

static int cmp_int(const void *a, const void *b) {
	return *(const int *)a - *(const int *)b;
}

static int cmp_str(const void *a, const void *b) {
	return strcmp(*(const char *const *)a, *(const char *const *)b);
}

TEST(qsort_ints) {
	int arr[] = {5, 3, 1, 4, 2};
	qsort(arr, 5, sizeof(int), cmp_int);
	for (int i = 0; i < 4; i++) {
		ASSERT(arr[i] <= arr[i + 1], "ascending order");
	}
	return 0;
}

TEST(qsort_strings) {
	const char *arr[] = {"banana", "apple", "cherry", "date"};
	qsort(arr, 4, sizeof(char *), cmp_str);
	ASSERT_EQ(strcmp(arr[0], "apple"), 0, "first");
	ASSERT_EQ(strcmp(arr[1], "banana"), 0, "second");
	ASSERT_EQ(strcmp(arr[2], "cherry"), 0, "third");
	ASSERT_EQ(strcmp(arr[3], "date"), 0, "fourth");
	return 0;
}

TEST(qsort_already_sorted) {
	int arr[] = {1, 2, 3, 4, 5};
	qsort(arr, 5, sizeof(int), cmp_int);
	for (int i = 0; i < 5; i++) {
		ASSERT_EQ(arr[i], i + 1, "unchanged");
	}
	return 0;
}

TEST(qsort_reverse) {
	int arr[] = {5, 4, 3, 2, 1};
	qsort(arr, 5, sizeof(int), cmp_int);
	for (int i = 0; i < 5; i++) {
		ASSERT_EQ(arr[i], i + 1, "reversed to ascending");
	}
	return 0;
}

TEST(qsort_single) {
	int arr[] = {42};
	qsort(arr, 1, sizeof(int), cmp_int);
	ASSERT_EQ(arr[0], 42, "single element unchanged");
	return 0;
}

TEST(qsort_empty) {
	int arr[] = {1};
	qsort(arr, 0, sizeof(int), cmp_int);
	ASSERT_EQ(arr[0], 1, "no crash on empty");
	return 0;
}

TEST(qsort_duplicates) {
	int arr[] = {3, 1, 2, 1, 3, 2};
	qsort(arr, 6, sizeof(int), cmp_int);
	ASSERT_EQ(arr[0], 1, "first");
	ASSERT_EQ(arr[1], 1, "second");
	ASSERT_EQ(arr[2], 2, "third");
	ASSERT_EQ(arr[3], 2, "fourth");
	ASSERT_EQ(arr[4], 3, "fifth");
	ASSERT_EQ(arr[5], 3, "sixth");
	return 0;
}

struct big_elem {
	int key;
	char padding[300];
};

static int cmp_big(const void *a, const void *b) {
	return ((const struct big_elem *)a)->key -
	       ((const struct big_elem *)b)->key;
}

TEST(qsort_large_elements) {
	struct big_elem arr[4];
	arr[0].key = 40;
	arr[1].key = 10;
	arr[2].key = 30;
	arr[3].key = 20;
	memset(arr[0].padding, 'a', 300);
	memset(arr[1].padding, 'b', 300);
	memset(arr[2].padding, 'c', 300);
	memset(arr[3].padding, 'd', 300);
	qsort(arr, 4, sizeof(struct big_elem), cmp_big);
	ASSERT_EQ(arr[0].key, 10, "first by key");
	ASSERT_EQ(arr[1].key, 20, "second by key");
	ASSERT_EQ(arr[2].key, 30, "third by key");
	ASSERT_EQ(arr[3].key, 40, "fourth by key");
	ASSERT_EQ(arr[0].padding[0], 'b', "payload followed key");
	ASSERT_EQ(arr[3].padding[0], 'a', "payload followed key");
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
	RUN_TEST(ispunct_symbols);
	RUN_TEST(ispunct_not);
	RUN_TEST(isalnum_alpha);
	RUN_TEST(isalnum_digit);
	RUN_TEST(isalnum_not);
	RUN_TEST(isxdigit_digits);
	RUN_TEST(isxdigit_hex);
	RUN_TEST(isxdigit_not);
	RUN_TEST(isupper_test);
	RUN_TEST(islower_test);
	RUN_TEST(tolower_test);
	RUN_TEST(toupper_test);
	RUN_TEST(strdup_basic);
	RUN_TEST(strdup_empty);
	RUN_TEST(strndup_basic);
	RUN_TEST(strndup_longer);
	RUN_TEST(strrchr_found);
	RUN_TEST(strrchr_not_found);
	RUN_TEST(strrchr_null_terminator);
	RUN_TEST(strtok_basic);
	RUN_TEST(strtok_multiple_delims);
	RUN_TEST(strtok_leading_delim);
	RUN_TEST(strtok_r_basic);
	RUN_TEST(strtok_r_nested);
	RUN_TEST(strcasecmp_equal);
	RUN_TEST(strcasecmp_less);
	RUN_TEST(strcasecmp_greater);
	RUN_TEST(strcasecmp_prefix);
	RUN_TEST(strncasecmp_equal_n);
	RUN_TEST(strncasecmp_diff_n);
	RUN_TEST(strncasecmp_zero_n);
	RUN_TEST(strtoul_decimal);
	RUN_TEST(strtoul_hex);
	RUN_TEST(strtoul_octal);
	RUN_TEST(strtoul_endptr);
	RUN_TEST(strtol_negative);
	RUN_TEST(strtol_zero);
	RUN_TEST(errno_initial);
	RUN_TEST(errno_set);
	RUN_TEST(strerror_success);
	RUN_TEST(strerror_enoent);
	RUN_TEST(strerror_einval);
	RUN_TEST(strerror_unknown);
	RUN_TEST(strerror_negative);
	RUN_TEST(mkstemp_basic);
	RUN_TEST(mkstemp_write_read);
	RUN_TEST(mkstemp_unique);
	RUN_TEST(mkstemp_invalid_template);
	RUN_TEST(atexit_register);
	RUN_TEST(time_returns_zero);
	RUN_TEST(time_sets_tloc);
	RUN_TEST(localtime_returns_valid);
	RUN_TEST(localtime_zeroed);
	RUN_TEST(isprint_printable);
	RUN_TEST(isprint_not_printable);
	RUN_TEST(isatty_console);
	RUN_TEST(isatty_file);
	RUN_TEST(getenv_unset);
	RUN_TEST(setenv_basic);
	RUN_TEST(setenv_overwrite);
	RUN_TEST(setenv_no_overwrite);
	RUN_TEST(setenv_invalid);
	RUN_TEST(unsetenv_basic);
	RUN_TEST(unsetenv_nonexistent);
	RUN_TEST(unsetenv_invalid);
	RUN_TEST(setenv_multiple);
	RUN_TEST(qsort_ints);
	RUN_TEST(qsort_strings);
	RUN_TEST(qsort_already_sorted);
	RUN_TEST(qsort_reverse);
	RUN_TEST(qsort_single);
	RUN_TEST(qsort_empty);
	RUN_TEST(qsort_duplicates);
	RUN_TEST(qsort_large_elements);
}
