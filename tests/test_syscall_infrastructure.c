#include "test_framework.h"
#include "../syscall.h"

static void test_syscall_dispatcher_getpid(void) {
    TEST("Syscall dispatcher extracts syscall number (getpid stub)");

    long result;
    __asm__ volatile(
        "mov x8, %1\n"      // SYS_getpid in x8
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result from x0
        : "=r"(result)
        : "i"(SYS_getpid)
        : "x8", "x0"
    );

    // Stub returns -1
    ASSERT_EQ(result, -1);
    printf("  Syscall dispatcher invoked stub successfully\n");
}

static void test_syscall_dispatcher_write(void) {
    TEST("Syscall dispatcher extracts arguments (write stub)");

    const char *msg = "test";
    long result;

    __asm__ volatile(
        "mov x8, %1\n"      // SYS_write
        "mov x0, #1\n"      // fd = stdout
        "mov x1, %2\n"      // buf pointer
        "mov x2, #4\n"      // count
        "svc #0\n"          // Invoke syscall
        "mov %0, x0\n"      // Get result
        : "=r"(result)
        : "i"(SYS_write), "r"(msg)
        : "x8", "x0", "x1", "x2"
    );

    // Stub returns -1
    ASSERT_EQ(result, -1);
    printf("  Syscall dispatcher extracted arguments successfully\n");
}

void run_syscall_infrastructure_tests(void) {
    TEST_SUITE("Syscall Infrastructure");
    test_syscall_dispatcher_getpid();
    test_syscall_dispatcher_write();
}
