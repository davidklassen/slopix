#include "test.h"

#ifdef RUN_TESTS

TEST(undefined_instruction) {
	__asm__ volatile(".word 0x00000000");
	return 0;
}

TEST(svc_call) {
	__asm__ volatile("svc #42");
	return 0;
}

#endif

TEST_SUITE(exception) {
	RUN_TEST(undefined_instruction);
	RUN_TEST(svc_call);
}
