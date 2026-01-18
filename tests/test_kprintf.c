#ifdef RUN_TESTS

#include "test.h"
#include "kprintf.h"

TEST(kprintf_string) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "hello %s", "world");
	ASSERT_STREQ(buf, "hello world");
	return 0;
}

TEST(kprintf_decimal) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%d", 42);
	ASSERT_STREQ(buf, "42");
	ksnprintf(buf, sizeof(buf), "%d", -123);
	ASSERT_STREQ(buf, "-123");
	ksnprintf(buf, sizeof(buf), "%d", 0);
	ASSERT_STREQ(buf, "0");
	return 0;
}

TEST(kprintf_unsigned) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%u", 42);
	ASSERT_STREQ(buf, "42");
	return 0;
}

TEST(kprintf_hex) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%x", 255);
	ASSERT_STREQ(buf, "ff");
	ksnprintf(buf, sizeof(buf), "%X", 255);
	ASSERT_STREQ(buf, "FF");
	ksnprintf(buf, sizeof(buf), "%x", 0);
	ASSERT_STREQ(buf, "0");
	return 0;
}

TEST(kprintf_pointer) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%p", (void *)0x1234);
	ASSERT_STREQ(buf, "0x0000000000001234");
	return 0;
}

TEST(kprintf_long) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%ld", -1L);
	ASSERT_STREQ(buf, "-1");
	ksnprintf(buf, sizeof(buf), "%lu", 123456789UL);
	ASSERT_STREQ(buf, "123456789");
	ksnprintf(buf, sizeof(buf), "%lx", 0xdeadbeefUL);
	ASSERT_STREQ(buf, "deadbeef");
	return 0;
}

TEST(kprintf_long_min) {
	char buf[32];
	ksnprintf(buf, sizeof(buf), "%ld", -9223372036854775807L - 1);
	ASSERT_STREQ(buf, "-9223372036854775808");
	return 0;
}

TEST(kprintf_char) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "%c", 'A');
	ASSERT_STREQ(buf, "A");
	return 0;
}

TEST(kprintf_percent) {
	char buf[64];
	ksnprintf(buf, sizeof(buf), "100%%");
	ASSERT_STREQ(buf, "100%");
	return 0;
}

TEST(kprintf_mixed) {
	char buf[128];
	ksnprintf(buf, sizeof(buf), "str=%s int=%d hex=0x%x", "test", 42, 255);
	ASSERT_STREQ(buf, "str=test int=42 hex=0xff");
	return 0;
}

TEST_SUITE(kprintf) {
	RUN_TEST(kprintf_string);
	RUN_TEST(kprintf_decimal);
	RUN_TEST(kprintf_unsigned);
	RUN_TEST(kprintf_hex);
	RUN_TEST(kprintf_pointer);
	RUN_TEST(kprintf_long);
	RUN_TEST(kprintf_long_min);
	RUN_TEST(kprintf_char);
	RUN_TEST(kprintf_percent);
	RUN_TEST(kprintf_mixed);
}

#endif
