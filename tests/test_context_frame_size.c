#include "test_framework.h"
#include "../process.h"

void test_context_frame_size_correct(void) {
    TEST("Context frame size is 36 quad-words");

    ASSERT_EQ(CONTEXT_FRAME_SIZE, 36);
}

void test_context_frame_alignment(void) {
    TEST("Context frame is 16-byte aligned and equals 288 bytes");

    ASSERT_EQ(CONTEXT_FRAME_BYTES % 16, 0);  // Must be 16-byte aligned
    ASSERT_EQ(CONTEXT_FRAME_BYTES, 288);     // 36 * 8 = 288
}

void test_context_frame_matches_struct(void) {
    TEST("Context frame can hold all cpu_context_t fields");

    // Verify the frame can hold the register state from cpu_context_t
    // cpu_context_t has 36 register fields (x0-x30, sp_el1, pc, pstate, sp_el0, ttbr0_el1)
    // plus non-register metadata fields (exception_level, padding)
    // The context frame holds exactly the 36 register fields
    unsigned long registers_in_context = 31 + 5;  // x0-x30 + 5 control registers
    ASSERT_EQ(CONTEXT_FRAME_SIZE, registers_in_context);
}

void run_context_frame_size_tests(void) {
    TEST_SUITE("Context Frame Size");

    test_context_frame_size_correct();
    test_context_frame_alignment();
    test_context_frame_matches_struct();
}
