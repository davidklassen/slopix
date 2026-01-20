#include <test.h>

int __tests_run = 0;
int __tests_failed = 0;

void run_test(const char *name, int (*fn)(void)) {
	puts("  ");
	puts(name);
	puts(": ");
	__tests_run++;
	if (fn() == 0) {
		puts("PASS\n");
	} else {
		__tests_failed++;
	}
}

void run_suite(const char *name, void (*fn)(void)) {
	puts("[");
	puts(name);
	puts("]\n");
	fn();
}

void test_report(void) {
	printf("\n=== Test Report ===\n");
	printf("  %d passed\n", __tests_run - __tests_failed);
	printf("  %d failed\n", __tests_failed);

	if (__tests_failed > 0) {
		puts("TESTS FAILED\n\n");
	} else {
		puts("ALL PASSED\n\n");
	}
}
