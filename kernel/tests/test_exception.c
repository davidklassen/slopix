#include "test.h"
#include "cpu.h"
#include "exception.h"

TEST(undefined_instruction) {
	asm volatile(".word 0x00000000");

	unsigned long esr = read_esr_el1();
	ASSERT_EQ(ESR_EC(esr), EC_UNKNOWN, "ESR_EC not EC_UNKNOWN");
	return 0;
}

TEST(svc_call) {
	asm volatile("svc #42");

	unsigned long esr = read_esr_el1();
	ASSERT_EQ(ESR_EC(esr), EC_SVC_AARCH64, "ESR_EC not EC_SVC_AARCH64");
	ASSERT_EQ(ESR_ISS(esr) & 0xFFFF, 42, "SVC immediate not 42");
	return 0;
}

TEST_SUITE(exception) {
	RUN_TEST(undefined_instruction);
	RUN_TEST(svc_call);
}
