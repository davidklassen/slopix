#include "test.h"
#include "kprintf.h"

#ifdef RUN_TESTS

int __tests_run = 0;
int __tests_failed = 0;

int test_streq(const char *a, const char *b) {
	while (*a && *b && *a == *b) {
		a++;
		b++;
	}
	return *a == *b;
}

void run_test(const char *name, int (*fn)(void)) {
	uart_puts("  ");
	uart_puts(name);
	uart_puts(": ");
	__tests_run++;
	if (fn() == 0) {
		uart_puts("PASS\n");
	} else {
		__tests_failed++;
	}
}

void run_suite(const char *name, void (*fn)(void)) {
	uart_puts("[");
	uart_puts(name);
	uart_puts("]\n");
	fn();
}

void test_report(void) {
	kprintf("\n=== Test Report ===\n");
	kprintf("  %d passed\n", __tests_run - __tests_failed);
	kprintf("  %d failed\n", __tests_failed);

	if (__tests_failed > 0) {
		kprintf("TESTS FAILED\n");
	} else {
		kprintf("ALL PASSED\n");
	}
}

#endif
