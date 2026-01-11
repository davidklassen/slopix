#include "test_framework.h"
#include "../printf.h"

static void test_svc_instruction(void) {
    TEST("SVC instruction is detected and handled");

    printf("  Executing SVC #0 instruction...\n");

    // Execute SVC #0
    __asm__ volatile("svc #0");

    // If we reach here, SVC was handled and returned successfully
    printf("  " COLOR_GREEN "[SVC] Returned from SVC successfully" COLOR_RESET "\n");
}

void run_svc_detection_tests(void) {
    TEST_SUITE("SVC Instruction Detection");
    test_svc_instruction();
}
