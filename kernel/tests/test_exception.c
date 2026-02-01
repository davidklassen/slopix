#ifdef RUN_TESTS

#include "test.h"

TEST(undefined_instruction) {
	asm volatile(".word 0x00000000");
	return 0;
}

TEST(svc_call) {
	asm volatile("svc #42");
	return 0;
}

TEST_SUITE(exception) {
	RUN_TEST(undefined_instruction);
	RUN_TEST(svc_call);
}

#endif
