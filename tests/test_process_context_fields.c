#include "test_framework.h"
#include "../process.h"
#include "../printf.h"

void test_context_has_el0_fields(void) {
    TEST("cpu_context_t has EL0 fields that can be set and read");

    cpu_context_t ctx;

    // Set the new EL0-specific fields
    ctx.sp_el0 = 0x1234;
    ctx.ttbr0_el1 = 0x5678;
    ctx.exception_level = 0;

    // Verify each field holds its value
    ASSERT_EQ(ctx.sp_el0, 0x1234);
    ASSERT_EQ(ctx.ttbr0_el1, 0x5678);
    ASSERT_EQ(ctx.exception_level, 0);
}

void test_context_structure_alignment(void) {
    TEST("cpu_context_t structure size is multiple of 8 bytes");

    unsigned long size = sizeof(cpu_context_t);
    ASSERT(size % 8 == 0, "Structure size is 8-byte aligned");

    printf("  [INFO] cpu_context_t size: %lu bytes\n", size);
}

void run_context_fields_tests(void) {
    TEST_SUITE("Process Context Fields");

    test_context_has_el0_fields();
    test_context_structure_alignment();
}
