#include "test_framework.h"
#include "../uart.h"

// Test function to trigger a synchronous exception (SVC)
// This test will trigger an exception and halt the system,
// so it should only be run when explicitly testing exception handling
void test_svc_exception(void) {
    TEST("SVC Exception Handling");

    printf("  [INFO] About to trigger SVC #99 exception\n");
    printf("  [INFO] Expected: EC=0x15 (SVC from AArch64)\n");
    printf("  [INFO] System should print exception info and halt gracefully\n");

    // Flush UART to ensure all output is sent before exception
    for (volatile int i = 0; i < 100000; i++);

    // Trigger SVC (Supervisor Call) exception
    // This should generate a synchronous exception with EC=0x15
    __asm__ volatile("svc #99");

    // Should never reach here - exception handler halts
    printf("  [ERROR] Should not reach here after SVC!\n");
}

void run_sync_exception_tests(void) {
    TEST_SUITE("Synchronous Exception Handler");

    // Note: This test will halt the system when it triggers the exception
    // It's meant to verify that the exception handler works correctly
    test_svc_exception();
}
