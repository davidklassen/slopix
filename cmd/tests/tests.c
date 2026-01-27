#include <test.h>

extern void test_suite_syscalls(void);
extern void test_suite_scheduler(void);
extern void test_suite_memory(void);
extern void test_suite_mmap(void);
extern void test_suite_filesys(void);
extern void test_suite_pipes(void);
extern void test_suite_libc(void);
extern void test_suite_devices(void);
extern void test_suite_codegen(void);
extern void test_suite_malloc(void);
extern void test_suite_stdio(void);

int main(void) {
	RUN_SUITE(libc);
	RUN_SUITE(syscalls);
	RUN_SUITE(scheduler);
	RUN_SUITE(memory);
	RUN_SUITE(mmap);
	RUN_SUITE(filesys);
	RUN_SUITE(pipes);
	RUN_SUITE(devices);
	RUN_SUITE(codegen);
	RUN_SUITE(malloc);
	RUN_SUITE(stdio);
	TEST_REPORT();
	TEST_EXIT();
	return 0;
}
