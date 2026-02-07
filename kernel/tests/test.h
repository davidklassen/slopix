#ifndef TEST_H
#define TEST_H

#include "uart.h"
#include "psci.h"

extern int runtests;
extern int __tests_run;
extern int __tests_failed;

void run_test(const char *name, int (*fn)(void));
void run_suite(const char *name, void (*fn)(void));
void test_report(void);

#define TEST(name)	    static int test_##name(void)
#define TEST_SUITE(name)    void test_suite_##name(void)
#define RUN_TEST(name)	    run_test(#name, test_##name)
#define DECLARE_SUITE(name) extern void test_suite_##name(void)
#define RUN_SUITE(name)	    run_suite(#name, test_suite_##name)
#define TEST_REPORT()	    test_report()
#define TEST_EXIT()                              \
	do {                                     \
		if (runtests) psci_system_off(); \
	} while (0)

#define ASSERT(cond, msg)                        \
	do {                                     \
		if (!(cond)) {                   \
			uart_puts("    FAIL: "); \
			uart_puts(msg);          \
			uart_puts("\n");         \
			return 1;                \
		}                                \
	} while (0)

#define ASSERT_EQ(a, b, msg)	ASSERT((a) == (b), msg)
#define ASSERT_NE(a, b, msg)	ASSERT((a) != (b), msg)
#define ASSERT_NOT_NULL(p, msg) ASSERT((p) != 0, msg)
#define ASSERT_NULL(p, msg)	ASSERT((p) == 0, msg)

int test_streq(const char *a, const char *b);

#define ASSERT_STREQ(actual, expected)                      \
	do {                                                \
		if (!test_streq(actual, expected)) {        \
			uart_puts("    FAIL: expected \""); \
			uart_puts(expected);                \
			uart_puts("\" got \"");             \
			uart_puts(actual);                  \
			uart_puts("\"\n");                  \
			return 1;                           \
		}                                           \
	} while (0)

#endif
