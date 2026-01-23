#ifdef RUN_TESTS

#include "test.h"
#include "cpu.h"
#include "vmm.h"
#include "pmm.h"
#include "string.h"

#define TEST_VA 0x200000

TEST(tlb_stale_after_unmap) {
	pte_t *pt = vmm_create();
	ASSERT_NOT_NULL(pt, "vmm_create");

	paddr_t pa1 = pmm_alloc();
	ASSERT(pa1 != PMM_INVALID, "pmm_alloc pa1");

	paddr_t pa2 = pmm_alloc();
	ASSERT(pa2 != PMM_INVALID, "pmm_alloc pa2");

	int r = vmm_map_page(pt, TEST_VA, pa1, 1, 0);
	ASSERT_EQ(r, 0, "vmm_map_page pa1");

	// Save old TTBR0 and switch to test page table
	unsigned long old_ttbr0 = read_ttbr0_el1();
	unsigned long flags = irq_save();

	write_ttbr0_el1(VA_TO_PA(pt));
	tlbi_vmalle1();

	// Write marker to first page via user VA
	volatile char *p = (volatile char *)TEST_VA;
	*p = 0xAB;
	ASSERT_EQ(*p, (char)0xAB, "wrote marker to pa1");

	paddr_t unmapped_pa;
	r = vmm_unmap_page(pt, TEST_VA, &unmapped_pa);
	ASSERT_EQ(r, 0, "vmm_unmap_page");
	ASSERT_EQ(unmapped_pa, pa1, "unmapped correct PA");

	// Map a NEW physical page at the same VA
	r = vmm_map_page(pt, TEST_VA, pa2, 1, 0);
	ASSERT_EQ(r, 0, "vmm_map_page pa2");

	// Zero the new page via kernel VA (bypasses TLB for user VA)
	memset(PA_TO_VA(pa2), 0, PAGE_SIZE);

	// Read via user VA - with stale TLB, this reads from pa1 (0xAB)
	// with proper TLB invalidation, this reads from pa2 (0x00)
	char val = *p;

	// Restore TTBR0 and interrupts before asserting
	write_ttbr0_el1(old_ttbr0);
	tlbi_vmalle1();
	irq_restore(flags);

	// Clean up
	vmm_free(pt);
	pmm_free(pa1);

	// This assertion will FAIL if TLB has stale entry (val == 0xAB)
	// It will PASS if TLB was properly invalidated (val == 0x00)
	ASSERT_EQ(val, 0, "stale TLB: read old data instead of zeroed page");

	return 0;
}

TEST_SUITE(tlb) {
	RUN_TEST(tlb_stale_after_unmap);
}

#endif
