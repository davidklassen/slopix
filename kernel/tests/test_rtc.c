#ifdef RUN_TESTS

#include "test.h"
#include "rtc.h"

TEST(rtc_periphid) {
	unsigned int id0 = RTC_REG(RTC_PERIPHID0);
	unsigned int id1 = RTC_REG(RTC_PERIPHID1);
	unsigned int part = (id0 & 0xFF) | ((id1 & 0x0F) << 8);
	ASSERT_EQ(part, 0x031, "PL031 part number");
	return 0;
}

TEST(rtc_enabled) {
	ASSERT(RTC_REG(RTC_CR) & 1, "RTC counter enabled");
	return 0;
}

TEST(rtc_reads_nonzero) {
	ASSERT(rtc_read() > 0, "RTC returns nonzero");
	return 0;
}

TEST(rtc_reads_reasonable) {
	ASSERT(rtc_read() > 1700000000, "RTC after 2023");
	return 0;
}

TEST_SUITE(rtc) {
	RUN_TEST(rtc_periphid);
	RUN_TEST(rtc_enabled);
	RUN_TEST(rtc_reads_nonzero);
	RUN_TEST(rtc_reads_reasonable);
}

#endif
