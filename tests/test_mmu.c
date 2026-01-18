#ifdef RUN_TESTS

#include "test.h"
#include "arch.h"
#include "mmu.h"
#include "uart.h"

TEST(mmu_enabled) {
	unsigned long sctlr = read_sctlr_el1();
	ASSERT(sctlr & SCTLR_M, "MMU should be enabled (M bit set)");
	return 0;
}

TEST(cache_enabled) {
	unsigned long sctlr = read_sctlr_el1();
	ASSERT(sctlr & SCTLR_C, "Data cache should be enabled (C bit set)");
	ASSERT(sctlr & SCTLR_I, "Instruction cache should be enabled (I bit set)");
	return 0;
}

TEST(tcr_configured) {
	unsigned long tcr = read_tcr_el1();
	unsigned long t0sz = tcr & 0x3F;
	unsigned long t1sz = (tcr >> 16) & 0x3F;
	ASSERT(t0sz == 16, "T0SZ should be 16 for 48-bit VA");
	ASSERT(t1sz == 16, "T1SZ should be 16 for 48-bit VA");
	return 0;
}

TEST(kernel_va_accessible) {
	volatile unsigned int *uart = (volatile unsigned int *)UART0_VIRT;
	(void)*uart;
	return 0;
}

TEST(identity_va_accessible) {
	volatile unsigned int *uart = (volatile unsigned int *)UART0_PHYS;
	(void)*uart;
	return 0;
}

TEST_SUITE(mmu) {
	RUN_TEST(mmu_enabled);
	RUN_TEST(cache_enabled);
	RUN_TEST(tcr_configured);
	RUN_TEST(kernel_va_accessible);
	RUN_TEST(identity_va_accessible);
}

#endif
