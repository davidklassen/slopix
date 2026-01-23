#ifdef RUN_TESTS

#include "test.h"
#include "cpu.h"
#include "vmm.h"
#include "uart.h"
#include "pmm.h"

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
	volatile unsigned int *uart = (volatile unsigned int *)UART0_VA;
	(void)*uart;
	return 0;
}

TEST(identity_va_accessible) {
	volatile unsigned int *uart = (volatile unsigned int *)UART0_PA;
	(void)*uart;
	return 0;
}

TEST(kernel_runs_from_high_address) {
	unsigned long pc = read_pc();
	ASSERT((pc >> 48) == 0xFFFF, "Kernel must run at high VA");
	return 0;
}

TEST(ttbr0_and_ttbr1_are_different) {
	unsigned long ttbr0 = read_ttbr0_el1();
	unsigned long ttbr1 = read_ttbr1_el1();
	// Use PTE_ADDR_MASK to extract base address (bits [47:12])
	ASSERT_NE(ttbr0 & PTE_ADDR_MASK, ttbr1 & PTE_ADDR_MASK, "Separate tables");
	return 0;
}

TEST(va_pa_roundtrip) {
	paddr_t pa = RAM_BASE + 0x1000;
	void *va = PA_TO_VA(pa);
	paddr_t pa2 = VA_TO_PA(va);
	ASSERT_EQ(pa, pa2, "Round-trip");
	return 0;
}

TEST_SUITE(vmm) {
	RUN_TEST(mmu_enabled);
	RUN_TEST(cache_enabled);
	RUN_TEST(tcr_configured);
	RUN_TEST(kernel_va_accessible);
	RUN_TEST(identity_va_accessible);
	RUN_TEST(kernel_runs_from_high_address);
	RUN_TEST(ttbr0_and_ttbr1_are_different);
	RUN_TEST(va_pa_roundtrip);
}

#endif
