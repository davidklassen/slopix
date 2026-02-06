# ISSUE_7: TLB Invalidation Barrier Ordering Bug in vmm_unmap_page()

## Severity
**Critical**

## Location
- **File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/vmm.c`
- **Lines:** 74-75
- **Related File:** `/Users/davidklassen/work/davidklassen/slopix/kernel/cpu.h`
- **Related Lines:** 88-92 (tlbi_va function)

## Description

The `vmm_unmap_page()` function clears a page table entry (PTE) at line 74, then calls `tlbi_va()` to invalidate the TLB at line 75. However, the `tlbi_va()` function in cpu.h (lines 88-92) executes the TLBI instruction followed by DSB and ISB barriers, but does **not** have a DSB **before** the TLBI.

According to the ARM Architecture specification (ARM Cortex-A Series Programmer's Guide for ARMv8-A, Chapter 11), the correct sequence for TLB invalidation must be:

```
DSB ISHST  // ensure PTE write is visible to page table walker
TLBI VAAE1IS  // invalidate TLB entry
DSB ISH    // ensure TLBI completion
ISB        // synchronize context
```

The current code structure in `tlbi_va()` is missing the **DSB before TLBI**, which violates ARM specifications. Without this barrier, the page table walker may not see the cleared PTE before the TLBI is issued, leading to unpredictable behavior.

## How to Reproduce

1. Call `vmm_unmap_page()` to unmap a user page
2. Access the same virtual address from user code after unmapping
3. Expected: Page fault (correct)
4. Actual: Undefined behavior or silent memory corruption (due to missing DSB before TLBI)

In low-stress testing this may not manifest, but in heavy workloads with concurrent page table modifications or under hardware optimizations that delay PTE writes, the bug could cause:
- Silent data corruption (memory access uses stale TLB entry)
- Crashes from TLB walker inconsistency
- Hypervisor/nested virtualization issues

## Test Recommendations

This is difficult to test deterministically in an automated test without modifying hardware behavior:

1. **Read-only test:** Add a test that unmaps a page and verifies the PTE is cleared via direct read (already read-only)
2. **Stress test:** Create a high-frequency page mapping/unmapping workload to increase likelihood of race conditions
3. **Formal specification check:** Review all TLBI call sites (not just `tlbi_va()`) to ensure consistent DSB placement

Note: The bug may only manifest on real hardware or under specific QEMU configurations that model ARM hardware faithfully.

## Fixing Recommendations

Add a DSB instruction **before** the TLBI in the `tlbi_va()` function to ensure the PTE write is visible to the page table walker.

**Current code (cpu.h, lines 88-92):**
```c
static inline void tlbi_va(unsigned long va) {
	asm volatile("tlbi vaae1is, %0" : : "r"(va >> 12));
	dsb();
	isb();
}
```

**Fixed code:**
```c
static inline void tlbi_va(unsigned long va) {
	dsb();  // ensure PTE write is visible to page table walker
	asm volatile("tlbi vaae1is, %0" : : "r"(va >> 12));
	dsb();  // ensure TLBI completion
	isb();
}
```

This follows the ARM specification pattern exactly and matches the reference pattern shown in the ARM Cortex-A Programmer's Guide.
