#include "test_framework.h"
#include "../syscall.h"
#include "../process.h"

static void dummy_test_fn(void) {
    // Empty test function for process creation
}

static void test_syscall_dispatcher_getpid(void) {
    TEST("Syscall: getpid() returns current process PID");

    // Create a test process and set it as current
    process_t *test_proc = process_create(dummy_test_fn, 4096);
    process_t *saved_current = process_get_current();
    process_set_current(test_proc);

    long result;
    __asm__ volatile(
        "mov x8, %1\n"      // SYS_getpid in x8
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result from x0
        : "=r"(result)
        : "i"(SYS_getpid)
        : "x8", "x0"
    );

    // Should return actual PID, not -1
    process_t *current = process_get_current();
    ASSERT_EQ(result, current->pid);
    printf("  getpid() returned PID=%ld\n", result);

    // Restore previous current process
    process_set_current(saved_current);
}

static void test_syscall_write_stdout(void) {
    TEST("Syscall: write() outputs to UART and returns byte count");

    const char *msg = "Hello from syscall!\n";
    long result;

    __asm__ volatile(
        "mov x8, %1\n"      // SYS_write
        "mov x0, #1\n"      // fd = stdout
        "mov x1, %2\n"      // buf pointer
        "mov x2, #20\n"     // count
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result
        : "=r"(result)
        : "i"(SYS_write), "r"(msg)
        : "x8", "x0", "x1", "x2"
    );

    ASSERT_EQ(result, 20);
    printf("  write() returned %ld bytes\n", result);
}

static void test_syscall_write_invalid_fd(void) {
    TEST("Syscall: write() rejects invalid file descriptor");

    const char *msg = "test";
    long result;

    __asm__ volatile(
        "mov x8, %1\n"      // SYS_write
        "mov x0, #99\n"     // fd = 99 (invalid)
        "mov x1, %2\n"      // buf pointer
        "mov x2, #4\n"      // count
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result
        : "=r"(result)
        : "i"(SYS_write), "r"(msg)
        : "x8", "x0", "x1", "x2"
    );

    ASSERT_EQ(result, -9);  // EBADF
    printf("  write() correctly rejected invalid fd\n");
}

void run_syscall_infrastructure_tests(void) {
    TEST_SUITE("Syscall Infrastructure");
    test_syscall_dispatcher_getpid();
    test_syscall_write_stdout();
    test_syscall_write_invalid_fd();
}
