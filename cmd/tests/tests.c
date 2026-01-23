#include <test.h>

extern void test_suite_syscalls(void);
extern void test_suite_scheduler(void);
extern void test_suite_memory(void);
extern void test_suite_filesys(void);
extern void test_suite_pipes(void);
extern void test_suite_libc(void);

int main(void) {
	RUN_SUITE(libc);
	RUN_SUITE(syscalls);
	RUN_SUITE(scheduler);
	RUN_SUITE(memory);
	RUN_SUITE(filesys);
	RUN_SUITE(pipes);
	TEST_REPORT();
	TEST_EXIT();
	return 0;
}
